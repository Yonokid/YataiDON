#include "input.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "animation.h"
#include "ime_ui.h"
#include "profiler.h"
#include "spdlog/spdlog.h"
#include "texture.h"
#include "time.h"

#ifndef __EMSCRIPTEN__
#include <libremidi/libremidi.hpp>
#ifdef _WIN32
#include <roapi.h>
#endif
#endif

// Only Windows and iOS have platform-specific keyboard helpers that input.cpp
// still calls directly: win32_is_key_down_native (native key state) on Windows,
// and the iOS screen-keyboard/touch-navigation functions. The Linux text-input
// handling used to live in platform_linux.cpp and has been folded into this
// file (handle_linux_text_input below).
#ifdef _WIN32
#include "../platform/platform_windows.h"
#elif defined(YATAIDON_PLATFORM_IOS)
#include "../platform/platform_ios.h"
#endif

#include "sdl_compat.h"
std::atomic<bool> input_thread_running{true};
std::thread input_thread;

static std::atomic<double> last_input_ms{0.0};

static std::unordered_map<SDL_JoystickID, SDL_Joystick*> sdl_joysticks;
// Joysticks with an SDL gamepad mapping, ordered by id; position is the slot in previous_gamepad_states.
static std::map<SDL_JoystickID, SDL_Gamepad*> sdl_gamepads;
// keyed by joy_id * 256 + button_index
static std::unordered_map<int64_t, bool> sdl_prev_button;
// keyed by joy_id * 256 + axis_index
static std::unordered_map<int64_t, float> sdl_prev_axis;
static bool sdl_joysticks_init_done = false;

static void refresh_sdl_joysticks() {
    bool& init_done = sdl_joysticks_init_done;
    if (!init_done) {
        if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
            spdlog::error("Failed to init SDL joystick subsystem: {}", SDL_GetError());
            return; // retry on the next call instead of latching a failed init
        }
        init_done = true;
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    std::unordered_set<SDL_JoystickID> current(ids, ids + count);

    for (auto it = sdl_joysticks.begin(); it != sdl_joysticks.end();) {
        if (!current.contains(it->first)) {
            SDL_CloseJoystick(it->second);
            it = sdl_joysticks.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = sdl_gamepads.begin(); it != sdl_gamepads.end();) {
        if (!current.contains(it->first)) {
            SDL_CloseGamepad(it->second);
            it = sdl_gamepads.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < count; i++) {
        if (SDL_IsGamepad(ids[i])) {
            if (!sdl_gamepads.contains(ids[i])) {
                SDL_Gamepad* gp = SDL_OpenGamepad(ids[i]);
                if (gp) {
                    sdl_gamepads[ids[i]] = gp;
                    spdlog::info("Gamepad: opened '{}' (id {})", SDL_GetGamepadName(gp), (int)ids[i]);
                }
            }
            continue;
        }
        if (!sdl_joysticks.contains(ids[i])) {
            SDL_Joystick* joy = SDL_OpenJoystick(ids[i]);
            if (joy) {
                sdl_joysticks[ids[i]] = joy;
                spdlog::info("Joystick fallback: opened '{}' (id {})", SDL_GetJoystickName(joy), (int)ids[i]);
            }
        }
    }

    SDL_free(ids);
}

// Not exposed in input.h: every access must go through check_key_pressed/
// check_key_released/clear_input_buffers below, which hold input_mutex --
// keeping these file-local prevents external code from touching the
// containers without the lock.
static std::mutex input_mutex;
static std::unordered_multimap<int, float> pressed_keys;
static std::unordered_multiset<int> released_keys;

static const int TOUCH_L_KAT = 40001;
static const int TOUCH_R_KAT = 40002;
static const int TOUCH_L_DON = 40003;
static const int TOUCH_R_DON = 40004;

static std::unordered_map<SDL_FingerID, int> touch_id_to_vkey;

std::atomic<bool> touch_drum_pressed{false};

// Track if touch drum is enabled (drawing + input, controlled by screens)
static std::atomic<bool> touch_drum_enabled{true};

void set_touch_drum_enabled(bool enabled) {
    touch_drum_enabled.store(enabled, std::memory_order_relaxed);
}

static std::array<bool, SDL_SCANCODE_COUNT> previous_key_states{};
static std::array<std::array<bool, 18>, 4> previous_gamepad_states{};

// Gamepad/joystick buttons and axes are folded into the key space at these
// offsets (config stores the bare button number).
static constexpr int GAMEPAD_VKEY_BASE = 10000;
static constexpr int AXIS_VKEY_BASE = 20000;
static std::atomic<int> last_gamepad_vkey{0};

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons, float* strength) {

    for (int key : keys) {
        if (check_key_pressed(key, strength)) {
            return true;
        }
    }

    // Check gamepad buttons (offset by 10000)
    if (gamepad_buttons.empty()) {
        return false;
    }

    return std::ranges::any_of(gamepad_buttons,
                               [strength](int button) { return check_key_pressed(10000 + button, strength); });
}

static bool keys_held(const std::vector<int>& keys, const std::vector<int>& pad) {
    for (int k : keys) {
        if (k >= 0 && k < (int)previous_key_states.size() && previous_key_states[k]) {
            return true;
        }
    }
    for (int b : pad) {
        if (b < 0 || b >= 18) {
            continue;
        }
        for (auto& g : previous_gamepad_states) {
            if (g[b]) {
                return true;
            }
        }
    }
    return false;
}

bool is_kat_held(bool left, PlayerNum player_num) {
    const auto& c = *global_data.config;
    bool p1 = player_num == PlayerNum::P1 || player_num == PlayerNum::ALL;
    bool p2 = player_num == PlayerNum::P2 || player_num == PlayerNum::ALL;
    return (p1 && keys_held(left ? c.keys_1p.left_kat : c.keys_1p.right_kat,
                            left ? c.gamepad_1p.left_kat : c.gamepad_1p.right_kat)) ||
           (p2 && keys_held(left ? c.keys_2p.left_kat : c.keys_2p.right_kat,
                            left ? c.gamepad_2p.left_kat : c.gamepad_2p.right_kat));
}

bool is_l_don_pressed(PlayerNum player_num, float* strength) {
    if (player_num == PlayerNum::P1) {
        return is_input_key_pressed(global_data.config->keys_1p.left_don, global_data.config->gamepad_1p.left_don,
                                    strength) ||
               check_key_pressed(TOUCH_L_DON, strength);
    }

    if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.left_don, global_data.config->gamepad_2p.left_don,
                                    strength);
    }

    if (player_num != PlayerNum::ALL) {
        return false;
    }
    std::vector<int> keys = global_data.config->keys_1p.left_don;
    const auto& keys_2p = global_data.config->keys_2p.left_don;
    keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    std::vector<int> gamepad_buttons = global_data.config->gamepad_1p.left_don;
    const auto& gp2 = global_data.config->gamepad_2p.left_don;
    gamepad_buttons.insert(gamepad_buttons.end(), gp2.begin(), gp2.end());
    return is_input_key_pressed(keys, gamepad_buttons, strength) || check_key_pressed(TOUCH_L_DON, strength);
}

bool is_r_don_pressed(PlayerNum player_num, float* strength) {
    if (player_num == PlayerNum::P1) {
        return is_input_key_pressed(global_data.config->keys_1p.right_don, global_data.config->gamepad_1p.right_don,
                                    strength) ||
               check_key_pressed(TOUCH_R_DON, strength);
    }

    if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.right_don, global_data.config->gamepad_2p.right_don,
                                    strength);
    }

    if (player_num != PlayerNum::ALL) {
        return false;
    }
    std::vector<int> keys = global_data.config->keys_1p.right_don;
    const auto& keys_2p = global_data.config->keys_2p.right_don;
    keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    std::vector<int> gamepad_buttons = global_data.config->gamepad_1p.right_don;
    const auto& gp2 = global_data.config->gamepad_2p.right_don;
    gamepad_buttons.insert(gamepad_buttons.end(), gp2.begin(), gp2.end());
    return is_input_key_pressed(keys, gamepad_buttons, strength) || check_key_pressed(TOUCH_R_DON, strength);
}

bool menu_left_pressed(PlayerNum p) {
    return is_l_kat_pressed(p) || (p != PlayerNum::P2 && key_pressed(SDL_SCANCODE_LEFT));
}

bool menu_right_pressed(PlayerNum p) {
    return is_r_kat_pressed(p) || (p != PlayerNum::P2 && key_pressed(SDL_SCANCODE_RIGHT));
}

bool menu_confirm_pressed(PlayerNum p) {
    return is_l_don_pressed(p) || is_r_don_pressed(p) || (p != PlayerNum::P2 && key_pressed(SDL_SCANCODE_RETURN));
}

bool is_l_kat_pressed(PlayerNum player_num, float* strength) {
    if (player_num == PlayerNum::P1) {
        return is_input_key_pressed(global_data.config->keys_1p.left_kat, global_data.config->gamepad_1p.left_kat,
                                    strength) ||
               check_key_pressed(TOUCH_L_KAT, strength);
    }

    if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.left_kat, global_data.config->gamepad_2p.left_kat,
                                    strength);
    }

    if (player_num != PlayerNum::ALL) {
        return false;
    }
    std::vector<int> keys = global_data.config->keys_1p.left_kat;
    const auto& keys_2p = global_data.config->keys_2p.left_kat;
    keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    std::vector<int> gamepad_buttons = global_data.config->gamepad_1p.left_kat;
    const auto& gp2 = global_data.config->gamepad_2p.left_kat;
    gamepad_buttons.insert(gamepad_buttons.end(), gp2.begin(), gp2.end());
    return is_input_key_pressed(keys, gamepad_buttons, strength) || check_key_pressed(TOUCH_L_KAT, strength);
}

bool is_r_kat_pressed(PlayerNum player_num, float* strength) {
    if (player_num == PlayerNum::P1) {
        return is_input_key_pressed(global_data.config->keys_1p.right_kat, global_data.config->gamepad_1p.right_kat,
                                    strength) ||
               check_key_pressed(TOUCH_R_KAT, strength);
    }

    if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.right_kat, global_data.config->gamepad_2p.right_kat,
                                    strength);
    }

    if (player_num != PlayerNum::ALL) {
        return false;
    }
    std::vector<int> keys = global_data.config->keys_1p.right_kat;
    const auto& keys_2p = global_data.config->keys_2p.right_kat;
    keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    std::vector<int> gamepad_buttons = global_data.config->gamepad_1p.right_kat;
    const auto& gp2 = global_data.config->gamepad_2p.right_kat;
    gamepad_buttons.insert(gamepad_buttons.end(), gp2.begin(), gp2.end());
    return is_input_key_pressed(keys, gamepad_buttons, strength) || check_key_pressed(TOUCH_R_KAT, strength);
}

static int touch_quadrant_vkey(Vec2 pos, int sw, int sh) {
    bool left = pos.x < sw / 2.0f;
    bool top = pos.y < sh / 2.0f;
    if (top) {
        return left ? TOUCH_L_KAT : TOUCH_R_KAT;
    }

    float render_scale = std::min((float)sw / tex.screen_width, (float)sh / tex.screen_height);
    float cx = sw * 0.5f;
    float cy = (float)sh * 0.5f + tex.screen_height * render_scale * 0.5f;

    float rx = tex.screen_width * 0.262f * render_scale;
    float ry = tex.screen_width * 0.242f * render_scale;

    float dx = pos.x - cx;
    float dy = pos.y - cy;

    float nx = dx / rx;
    float ny = dy / ry;
    bool in_drum = (nx * nx + ny * ny) <= 1.0f;
    if (in_drum) {
        return left ? TOUCH_L_DON : TOUCH_R_DON;
    }
    return left ? TOUCH_L_KAT : TOUCH_R_KAT;
}

static bool touch_watch_registered = false;

static int char_to_raylib_key(unsigned char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    if (c >= 32 && c <= 96) {
        return c;
    }
    return 0;
}

#if defined(__linux__) && !defined(__ANDROID__)
static bool handle_linux_text_input(SDL_Event* event) {
    if (is_input_locked()) {
        return false;
    }
    if (event->type != SDL_EVENT_TEXT_INPUT || !event->text.text) {
        return false;
    }

    const auto* key_state = SDL_GetKeyboardState(nullptr);
    std::scoped_lock lock(input_mutex);
    for (const char* p = event->text.text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        int key = char_to_raylib_key(c);
        if (!key) {
            continue;
        }
        SDL_Keycode keycode = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        SDL_Keymod mod = SDL_KMOD_NONE;
        SDL_Scancode sc = SDL_GetScancodeFromKey(keycode, &mod);
        if (sc != SDL_SCANCODE_UNKNOWN && key_state && key_state[sc]) {
            continue;
        }
        pressed_keys.emplace(key, 1.0f);
        released_keys.insert(key);
    }
    return true;
}
#endif

static YD_EventFilterRet SDLCALL touch_event_watch(void* /*userdata*/, SDL_Event* event) {
    if (is_input_locked()) {
        return 1;
    }

#if defined(__linux__) && !defined(__ANDROID__)
    if (handle_linux_text_input(event)) {
        return 1;
    }
#endif

    if (event->type == SDL_EVENT_KEY_DOWN && yd_key_scancode(*event) == SDL_SCANCODE_AC_BACK) {
        std::scoped_lock lock(input_mutex);
        pressed_keys.emplace(SDL_SCANCODE_ESCAPE, 1.0f);
        return 0;
    }
    if (event->type == SDL_EVENT_KEY_UP && yd_key_scancode(*event) == SDL_SCANCODE_AC_BACK) {
        std::scoped_lock lock(input_mutex);
        released_keys.insert(SDL_SCANCODE_ESCAPE);
        return 0;
    }

    if (event->type == SDL_EVENT_FINGER_DOWN) {
        if (!global_data.config || !global_data.config->general.touch_input) {
            return 1;
        }
        if (!touch_drum_enabled.load(std::memory_order_relaxed)) {
            return 1;
        }
        SDL_FingerID id = yd_finger_id(*event);
        std::scoped_lock lock(input_mutex);
        if (!touch_id_to_vkey.contains(id)) {
            int sw = get_screen_width();
            int sh = get_screen_height();
            Vec2 pos = {event->tfinger.x * sw, event->tfinger.y * sh};
            int vkey = touch_quadrant_vkey(pos, sw, sh);
#ifdef YATAIDON_PLATFORM_IOS
            vkey = ios_handle_touch_navigation(vkey, event->tfinger.x, event->tfinger.y);
#endif
            touch_id_to_vkey[id] = vkey;
            touch_drum_pressed.store(true, std::memory_order_relaxed);
            last_input_ms.store(get_current_ms(), std::memory_order_relaxed);
            pressed_keys.emplace(vkey, 1.0f);
        }
    } else if (event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED) {
        SDL_FingerID id = yd_finger_id(*event);
        std::scoped_lock lock(input_mutex);
        auto it = touch_id_to_vkey.find(id);
        if (it != touch_id_to_vkey.end()) {
            released_keys.insert(it->second);
            touch_id_to_vkey.erase(it);
        }
        touch_drum_pressed.store(!touch_id_to_vkey.empty(), std::memory_order_relaxed);
    }
    return 1;
}

bool draw_touch_drum() {
    if (!touch_drum_enabled.load(std::memory_order_relaxed)) {
        return false;
    }
    if (!global_data.config || !global_data.config->general.touch_input) {
        return false;
    }

    auto* touch_drum_resize = static_cast<TextureResizeAnimation*>(global_tex.get_animation(66));
    if (!touch_drum_resize) {
        return false;
    }

    if (!touch_drum_resize->isStarted()) {
        touch_drum_resize->start();
    }
    if (touch_drum_pressed.exchange(false, std::memory_order_relaxed)) {
        touch_drum_resize->restart();
    }
    touch_drum_resize->update(get_current_ms());
    const float scale = (float)touch_drum_resize->attribute;
    float y_fix = 0.0f;
    auto drum_it = global_tex.textures.find("overlay/touch_drum");
    if (drum_it != global_tex.textures.end()) {
        y_fix = drum_it->second->height * 0.5f * (1.0f - scale);
    }
    global_tex.draw_texture(global_tex.get_texture("overlay/touch_drum"),
                            {.scale = scale, .center = true, .y = y_fix, .fade = 0.5f});
#ifdef YATAIDON_PLATFORM_IOS
    ios_draw_touch_navigation_labels(static_cast<float>(get_screen_width()), static_cast<float>(get_screen_height()));
#endif
    return true;
}

void poll_touch_once() {
    if (!touch_watch_registered) {
        SDL_AddEventWatch(touch_event_watch, nullptr);
        touch_watch_registered = true;
    }
}

// Scan all keyboard keys once and push press/release events.
// Used by the polling thread on desktop and called directly per-frame on web.
void poll_keyboard_once() {
    if (is_input_locked()) {
        return;
    }
    thread_local std::vector<int> local_pressed;
    thread_local std::vector<int> local_released;
    local_pressed.clear();
    local_released.clear();

#ifdef _WIN32
    for (int key = 0; key < SDL_SCANCODE_COUNT; key++) {
        bool current_state = win32_is_key_down_native(key);
        bool previous_state = previous_key_states[key];
        if (current_state && !previous_state) {
            local_pressed.push_back(key);
        }
        if (!current_state && previous_state) {
            local_released.push_back(key);
        }
        previous_key_states[key] = current_state;
    }
#else
    int num_keys = 0;
    const auto* kb_state = SDL_GetKeyboardState(&num_keys);
    for (int key = 0; key < std::min(num_keys, (int)SDL_SCANCODE_COUNT); key++) {
        bool current_state = kb_state[key];
        bool previous_state = previous_key_states[key];
        if (current_state && !previous_state) {
            local_pressed.push_back(key);
        }
        if (!current_state && previous_state) {
            local_released.push_back(key);
        }
        previous_key_states[key] = current_state;
    }
#endif

    refresh_sdl_joysticks();

    // Button numbers follow raylib's GamepadButton enum, which is what config.toml stores.
    static constexpr std::pair<SDL_GamepadButton, int> GAMEPAD_BUTTONS[] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, 1},       {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 2},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 3},     {SDL_GAMEPAD_BUTTON_DPAD_LEFT, 4},
        {SDL_GAMEPAD_BUTTON_NORTH, 5},         {SDL_GAMEPAD_BUTTON_EAST, 6},
        {SDL_GAMEPAD_BUTTON_SOUTH, 7},         {SDL_GAMEPAD_BUTTON_WEST, 8},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 9}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 11},
        {SDL_GAMEPAD_BUTTON_BACK, 13},         {SDL_GAMEPAD_BUTTON_GUIDE, 14},
        {SDL_GAMEPAD_BUTTON_START, 15},        {SDL_GAMEPAD_BUTTON_LEFT_STICK, 16},
        {SDL_GAMEPAD_BUTTON_RIGHT_STICK, 17},
    };
    constexpr float TRIGGER_THRESHOLD = 0.1f;

    int gamepad = 0;
    for (auto& [id, gp] : sdl_gamepads) {
        if (gamepad >= 4) {
            break;
        }
        std::array<bool, 18> state{};
        for (const auto& [sdl_btn, btn] : GAMEPAD_BUTTONS) {
            state[btn] = SDL_GetGamepadButton(gp, sdl_btn);
        }
        state[10] = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f > TRIGGER_THRESHOLD;
        state[12] = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f > TRIGGER_THRESHOLD;
        for (int btn = 1; btn <= 17; btn++) {
            int key = 10000 + btn;
            bool previous_state = previous_gamepad_states[gamepad][btn];
            if (state[btn] && !previous_state) {
                local_pressed.push_back(key);
            }
            if (!state[btn] && previous_state) {
                local_released.push_back(key);
            }
            previous_gamepad_states[gamepad][btn] = state[btn];
        }
        gamepad++;
    }

    constexpr float JOYSTICK_AXIS_THRESHOLD = 0.5f;

    for (auto& [joy_id, joy] : sdl_joysticks) {
        int num_buttons = SDL_GetNumJoystickButtons(joy);
        for (int btn = 0; btn < num_buttons && btn < 32; btn++) {
            int64_t state_key = (int64_t)joy_id * 256 + btn;
            // Use 1-indexed encoding to match raylib's gamepad button convention
            int vkey = 10000 + btn + 1;
            bool cur = SDL_GetJoystickButton(joy, btn);
            bool prev = sdl_prev_button[state_key];
            if (cur && !prev) {
                local_pressed.push_back(vkey);
            }
            if (!cur && prev) {
                local_released.push_back(vkey);
            }
            sdl_prev_button[state_key] = cur;
        }

        int num_axes = SDL_GetNumJoystickAxes(joy);
        for (int axis = 0; axis < num_axes && axis < 8; axis++) {
            int64_t state_key = (int64_t)joy_id * 256 + axis;
            float value = SDL_GetJoystickAxis(joy, axis) / 32767.0f;
            float prev = sdl_prev_axis[state_key];
            int key_pos = 20000 + axis * 2;
            int key_neg = 20000 + axis * 2 + 1;
            bool cur_pos = value > JOYSTICK_AXIS_THRESHOLD;
            bool cur_neg = value < -JOYSTICK_AXIS_THRESHOLD;
            bool prev_pos = prev > JOYSTICK_AXIS_THRESHOLD;
            bool prev_neg = prev < -JOYSTICK_AXIS_THRESHOLD;
            if (cur_pos && !prev_pos) {
                local_pressed.push_back(key_pos);
            }
            if (!cur_pos && prev_pos) {
                local_released.push_back(key_pos);
            }
            if (cur_neg && !prev_neg) {
                local_pressed.push_back(key_neg);
            }
            if (!cur_neg && prev_neg) {
                local_released.push_back(key_neg);
            }
            sdl_prev_axis[state_key] = value;
        }
    }

    // Remember the newest controller press separately from pressed_keys so
    // the keybind screen can read it: the pressed_keys entry is consumed by
    // whatever navigates the menu before the option box ever sees it.
    for (int vkey : local_pressed) {
        if (vkey >= GAMEPAD_VKEY_BASE && vkey < AXIS_VKEY_BASE) {
            last_gamepad_vkey.store(vkey, std::memory_order_relaxed);
            break;
        }
    }

    if (!local_pressed.empty()) {
        last_input_ms.store(get_current_ms(), std::memory_order_relaxed);
    }

    if (!local_pressed.empty() || !local_released.empty()) {
        std::scoped_lock lock(input_mutex);
        for (int key : local_pressed) {
            pressed_keys.emplace(key, 1.0f);
        }
        released_keys.insert(local_released.begin(), local_released.end());
    }
}

double get_last_input_ms() {
    return last_input_ms.load(std::memory_order_relaxed);
}

int take_gamepad_button_pressed() {
    int vkey = last_gamepad_vkey.exchange(0, std::memory_order_relaxed);
    if (vkey < GAMEPAD_VKEY_BASE || vkey >= AXIS_VKEY_BASE) {
        return -1;
    }
    return vkey - GAMEPAD_VKEY_BASE;
}

void submit_gamepad_button_press(int button, float strength) {
    if (is_input_locked()) {
        return;
    }

    // The SDL joystick fallback currently exposes at most 32 buttons.
    if (button < 1 || button > 32) {
        return;
    }

    const int vkey = GAMEPAD_VKEY_BASE + button;
    last_gamepad_vkey.store(vkey, std::memory_order_relaxed);
    last_input_ms.store(get_current_ms(), std::memory_order_relaxed);

    std::scoped_lock lock(input_mutex);
    pressed_keys.emplace(vkey, std::max(strength, 0.0f));
}

#ifndef __EMSCRIPTEN__
namespace {

    struct MidiPortEvent {
        libremidi::input_port port;
        bool added;
    };

    struct MidiInput {
        std::string device;
        int channel = 0;
        std::unordered_map<int, int> note_to_button;
        std::mutex events_mutex;
        std::vector<MidiPortEvent> events;
        std::atomic<bool> events_pending{false};
        std::optional<libremidi::input_port> selected;
        std::unique_ptr<libremidi::midi_in> port;
        std::unique_ptr<libremidi::observer> observer;
#ifdef _WIN32
        bool winrt_initialized = false;

        ~MidiInput() {
            observer.reset();
            port.reset();
            if (winrt_initialized) {
                RoUninitialize();
            }
        }
#endif
    };

    std::unique_ptr<MidiInput> midi_input;

#ifdef _WIN32
    constexpr auto midi_api = libremidi::API::WINDOWS_UWP;
#else
    constexpr auto midi_api = libremidi::midi1::default_api();
#endif

    void midi_message_received(const libremidi::message& message, const MidiInput& state) {
        const auto& bytes = message.bytes;
        if (bytes.size() != 3 || (bytes[0] & 0xf0) != 0x90 || bytes[2] == 0) {
            return;
        }

        const int channel = (bytes[0] & 0x0f) + 1;
        if (state.channel != 0 && state.channel != channel) {
            return;
        }

        const auto mapping = state.note_to_button.find(bytes[1]);
        if (mapping != state.note_to_button.end()) {
            constexpr float minimum_strength = 0.5f;
            constexpr float maximum_strength = 1.15f;
            constexpr int normal_velocity = 80;
            const float strength =
                bytes[2] <= normal_velocity
                    ? minimum_strength + (1.0f - minimum_strength) * (bytes[2] - 1) / (normal_velocity - 1)
                    : 1.0f + (maximum_strength - 1.0f) * (bytes[2] - normal_velocity) / (127 - normal_velocity);

            submit_gamepad_button_press(mapping->second, strength);
        }
    }

    // Device notifications can arrive on background threads; open and close ports on the main thread.
    void queue_midi_port_event(MidiInput& state, const libremidi::input_port& port, bool added) {
        std::scoped_lock lock(state.events_mutex);
        state.events.push_back({port, added});
        state.events_pending.store(true, std::memory_order_release);
    }

} // namespace
#endif

void start_midi_input(const MidiConfig& config) {
    if (config.device.empty()) {
        return;
    }
#ifndef __EMSCRIPTEN__
    if (midi_input) {
        spdlog::warn("MIDI input listener was already started");
        return;
    }

    auto state = std::make_unique<MidiInput>();
    state->device = config.device;
    state->channel = config.channel;
    const std::size_t mapping_count = std::min(config.notes.size(), config.buttons.size());

    if (config.notes.size() != config.buttons.size()) {
        spdlog::warn("MIDI notes/buttons have different lengths; using the first {} pairs", mapping_count);
    }

    for (std::size_t i = 0; i < mapping_count; ++i) {
        const int note = config.notes[i];
        const int button = config.buttons[i];

        if (note < 0 || note > 127 || button < 1 || button > 32) {
            spdlog::warn("Skipping invalid MIDI mapping: note {} -> gamepad button {}", note, button);
            continue;
        }

        if (!state->note_to_button.emplace(note, button).second) {
            spdlog::warn("Duplicate MIDI note {} ignored after its first mapping", note);
        }
    }

    if (state->note_to_button.empty()) {
        spdlog::warn("MIDI input is active with no valid note/button mappings");
    }

#ifdef _WIN32
    HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
    if (result == RPC_E_CHANGED_MODE) {
        result = RoInitialize(RO_INIT_SINGLETHREADED);
    }

    if (FAILED(result)) {
        spdlog::warn("Could not initialize Windows MIDI services: {}", static_cast<long>(result));
        return;
    }

    state->winrt_initialized = true;
#endif
    auto* listener = state.get();
    libremidi::observer_configuration observer_config;

    observer_config.on_error = [](std::string_view error, const libremidi::source_location&) {
        spdlog::warn("MIDI device observer: {}", error);
    };

    observer_config.input_added = [listener](const libremidi::input_port& port) {
        queue_midi_port_event(*listener, port, true);
    };

    observer_config.input_removed = [listener](const libremidi::input_port& port) {
        queue_midi_port_event(*listener, port, false);
    };

    observer_config.track_virtual = true;
    observer_config.track_any = true;

    state->observer =
        std::make_unique<libremidi::observer>(observer_config, libremidi::observer_configuration_for(midi_api));

    if (state->observer->get_current_api() != midi_api) {
        spdlog::warn("MIDI backend is unavailable");
        return;
    }

    midi_input = std::move(state);
    process_midi_events();

    if (!midi_input->port) {
        spdlog::warn("No MIDI input port matches device substring '{}'; waiting for device", config.device);
    }
#else
    spdlog::warn("MIDI input is not supported on this platform");
#endif
}

void process_midi_events() {
#ifndef __EMSCRIPTEN__
    if (!midi_input || !midi_input->events_pending.exchange(false, std::memory_order_acquire)) {
        return;
    }
    auto& state = *midi_input;
    std::vector<MidiPortEvent> events;

    {
        std::scoped_lock lock(state.events_mutex);
        events.swap(state.events);
    }

    for (const auto& event : events) {
        if (!event.added) {
            if (state.selected && state.selected->api == event.port.api && state.selected->port == event.port.port &&
                state.selected->port_name == event.port.port_name) {
                state.port.reset();
                state.selected.reset();
                spdlog::info("MIDI input disconnected from '{}'", event.port.display_name);
            }

            continue;
        }

        spdlog::info("MIDI input available: {}", event.port.display_name);

        if (state.port ||
            (!event.port.display_name.contains(state.device) && !event.port.device_name.contains(state.device))) {
            continue;
        }

        libremidi::input_configuration input_config;

        input_config.on_message = [&state](const libremidi::message& message) {
            midi_message_received(message, state);
        };

        input_config.on_error = [](std::string_view error, const libremidi::source_location&) {
            spdlog::warn("MIDI input: {}", error);
        };

        auto port = std::make_unique<libremidi::midi_in>(input_config, libremidi::midi_in_configuration_for(midi_api));

        const auto error = port->open_port(event.port, "YataiDON MIDI Input");

        if (error != stdx::error{}) {
            const auto description = error.message();

            spdlog::warn("Could not connect MIDI input '{}': {}", event.port.display_name,
                         std::string_view(description.data(), description.size()));

            continue;
        }

        state.selected = event.port;
        state.port = std::move(port);

        spdlog::info("MIDI input connected to '{}' (channel {})", event.port.display_name,
                     state.channel == 0 ? "any" : std::to_string(state.channel));
    }
#endif
}

void shutdown_midi_input() {
#ifndef __EMSCRIPTEN__
    midi_input.reset();
#endif
}

void input_polling_thread() {
    PROFILE_THREAD_NAME("input");
    while (input_thread_running) {
        poll_keyboard_once();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

bool check_key_pressed(int key, float* strength) {
    std::scoped_lock lock(input_mutex);
    auto it = pressed_keys.find(key);
    if (it != pressed_keys.end()) {
        if (strength) {
            *strength = it->second;
        }
        pressed_keys.erase(it);
        return true;
    }
    return false;
}

bool check_key_released(int key) {
    std::scoped_lock lock(input_mutex);
    auto it = released_keys.find(key);
    if (it != released_keys.end()) {
        released_keys.erase(it);
        return true;
    }
    return false;
}

void clear_input_buffers() {
    std::scoped_lock lock(input_mutex);
    pressed_keys.clear();
    released_keys.clear();
}

// Caller must join input_polling_thread (which owns refresh_sdl_joysticks())
// before calling this -- sdl_joysticks is unsynchronized, so concurrent
// access here would race on the map and use-after-free a closed joystick.
void shutdown_sdl_joysticks() {
    for (auto& [id, joy] : sdl_joysticks) {
        SDL_CloseJoystick(joy);
    }
    sdl_joysticks.clear();
    for (auto& [id, gp] : sdl_gamepads) {
        SDL_CloseGamepad(gp);
    }
    sdl_gamepads.clear();
    sdl_prev_button.clear();
    sdl_prev_axis.clear();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    sdl_joysticks_init_done = false;
}

static bool keyboard_editing = false;

bool is_text_input_key(int key) {
    if (!keyboard_editing) {
        return false;
    }

    return (key >= SDL_SCANCODE_A &&
            key <= SDL_SCANCODE_SLASH) || // letters, digits, enter/esc/bksp/tab/space, punctuation
           (key >= SDL_SCANCODE_INSERT && key <= SDL_SCANCODE_UP) ||
           (key >= SDL_SCANCODE_KP_DIVIDE && key <= SDL_SCANCODE_KP_PERIOD) || key == SDL_SCANCODE_KP_EQUALS ||
           (key >= SDL_SCANCODE_LCTRL && key <= SDL_SCANCODE_RGUI);
}

void set_keyboard_visible(bool visible) {
    keyboard_editing = visible;
#if defined(PLATFORM_ANDROID) || defined(YATAIDON_PLATFORM_IOS)
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    if (!windows || count == 0) {
        return;
    }
    SDL_Window* win = windows[0];
    SDL_free(windows);
    if (visible) {
        SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "1");
        SDL_StartTextInput(win);
    } else {
        SDL_StopTextInput(win);
        SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    }
#else
    (void)visible;
#endif
}

// Unified keyboard text-field editing. See the declaration in input.h.
// raylib's SDL platform starts text input with the window (for typed characters). With an IME
// such as a Chinese one, every key press then goes through the IME, and PollInputEvents took
// 5-150 ms on the frame after a drum hit on a keyboard-type controller. Text input is only on
// while poll_text_edit is in use.
static bool text_input_wanted = false;
static bool text_input_active = true; // raylib started it

void sync_text_input() {
#if defined(PLATFORM_DESKTOP_SDL)
    if (text_input_wanted != text_input_active) {
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        if (windows && count > 0) {
            if (text_input_wanted) {
                SDL_StartTextInput(windows[0]);
            } else {
                SDL_StopTextInput(windows[0]);
            }
            text_input_active = text_input_wanted;
            ime_ui_text_input_changed(text_input_active);
        }
        SDL_free(windows);
    }
#endif
    text_input_wanted = false;
}

TextEditAction poll_text_edit(std::string& text, const std::function<bool(int)>& accept) {
    text_input_wanted = true;
    // Ctrl+V pastes the clipboard (first line only, UTF-8).
    if ((key_down(SDL_SCANCODE_LCTRL) || key_down(SDL_SCANCODE_RCTRL)) && key_pressed(SDL_SCANCODE_V)) {
        char* clip = SDL_GetClipboardText();
        if (clip) {
            std::string s(clip);
            SDL_free(clip);
            const size_t eol = s.find_first_of("\r\n");
            if (eol != std::string::npos) {
                s.resize(eol);
            }
            text += s;
        }
        while (char_pressed() > 0) {
        } // the 'v' itself is not typed
        return TextEditAction::None;
    }

    TextEditAction action = TextEditAction::None;
    if (key_pressed(SDL_SCANCODE_BACKSPACE)) {
        // Remove one whole UTF-8 code point (continuation bytes first).
        while (!text.empty() && ((unsigned char)text.back() & 0xC0) == 0x80) {
            text.pop_back();
        }
        if (!text.empty()) {
            text.pop_back();
        }
    } else if (key_pressed(SDL_SCANCODE_ESCAPE)) {
        action = TextEditAction::Cancel;
    } else if (key_pressed(SDL_SCANCODE_RETURN)) {
        action = TextEditAction::Confirm;
    }

    int key = char_pressed();
    while (key > 0) {
        if (key == '\n' || key == '\r') {
            if (action == TextEditAction::None) {
                action = TextEditAction::Confirm;
            }
        } else if (key >= 0x20 && (!accept || accept(key))) {
            // char_pressed yields a Unicode code point (IME-composed CJK
            // included); store it as UTF-8.
            int n = 0;
            const char* utf8 = codepoint_to_utf8(key, &n);
            text.append(utf8, n);
        }
        key = char_pressed();
    }
    return action;
}

// --- Raw keyboard / mouse state ------------------------------------------------------------
// Events are accumulated by an SDL event watch into `pending`; poll_input_events() publishes
// them as `frame` right after pumping, so reads see exactly this frame's input (and an event
// that lands between pumps is carried over rather than lost).
namespace {
    struct FrameInput {
        std::bitset<SDL_SCANCODE_COUNT> key_pressed;
        std::deque<int> key_queue;
        std::deque<int> char_queue;
        std::bitset<8> mouse_pressed;
        float wheel = 0.0f;
    };

    std::mutex frame_mutex;
    FrameInput pending;
    FrameInput frame;
    bool frame_watch_registered = false;

    // Decode one UTF-8 code point from the front of `s`, advancing it.
    int next_codepoint(const char*& s) {
        const auto* p = reinterpret_cast<const unsigned char*>(s);
        int len = p[0] < 0x80 ? 1 : p[0] < 0xE0 ? 2 : p[0] < 0xF0 ? 3 : 4;
        int cp = len == 1 ? p[0] : p[0] & (0xFF >> (len + 1));
        for (int i = 1; i < len && p[i]; i++) {
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        s += len;
        return cp;
    }

    YD_EventFilterRet SDLCALL frame_event_watch(void* /*userdata*/, SDL_Event* event) {
        std::scoped_lock lock(frame_mutex);
        switch (event->type) {
        case SDL_EVENT_KEY_DOWN: {
            const int sc = yd_key_scancode(*event);
            if (!event->key.repeat && sc > 0 && sc < SDL_SCANCODE_COUNT) {
                pending.key_pressed.set(sc);
                pending.key_queue.push_back(sc);
            }
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (event->text.text) {
                const char* s = event->text.text;
                while (*s) {
                    pending.char_queue.push_back(next_codepoint(s));
                }
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            pending.wheel += (float)event->wheel.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button < pending.mouse_pressed.size()) {
                pending.mouse_pressed.set(event->button.button);
            }
            break;
        default:
            break;
        }
        return 0;
    }
} // namespace

void poll_input_events() {
    if (!frame_watch_registered) {
        SDL_AddEventWatch(frame_event_watch, nullptr);
        frame_watch_registered = true;
    }
    ray::PollInputEvents();
    std::scoped_lock lock(frame_mutex);
    frame = std::move(pending);
    pending = FrameInput{};
}

bool key_down(int scancode) {
    int n = 0;
    const auto* state = SDL_GetKeyboardState(&n);
    return scancode >= 0 && scancode < n && state[scancode];
}

bool key_pressed(int scancode) {
    std::scoped_lock lock(frame_mutex);
    return scancode >= 0 && scancode < SDL_SCANCODE_COUNT && frame.key_pressed.test(scancode);
}

int key_pressed_next() {
    std::scoped_lock lock(frame_mutex);
    if (frame.key_queue.empty()) {
        return 0;
    }
    const int sc = frame.key_queue.front();
    frame.key_queue.pop_front();
    return sc;
}

int char_pressed() {
    std::scoped_lock lock(frame_mutex);
    if (frame.char_queue.empty()) {
        return 0;
    }
    const int cp = frame.char_queue.front();
    frame.char_queue.pop_front();
    return cp;
}

float mouse_wheel() {
    std::scoped_lock lock(frame_mutex);
    return frame.wheel;
}

Vec2 mouse_position() {
    float x = 0.0f, y = 0.0f;
#ifndef YATAIDON_USE_SDL2
    SDL_GetMouseState(&x, &y);
#else
    int ix = 0, iy = 0;
    SDL_GetMouseState(&ix, &iy);
    x = (float)ix;
    y = (float)iy;
#endif
    return {x, y};
}

bool mouse_down(int button) {
    return (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(button)) != 0;
}

bool mouse_pressed(int button) {
    std::scoped_lock lock(frame_mutex);
    return button >= 0 && button < (int)frame.mouse_pressed.size() && frame.mouse_pressed.test(button);
}

void show_cursor() {
#ifndef YATAIDON_USE_SDL2
    SDL_ShowCursor();
#else
    SDL_ShowCursor(SDL_ENABLE);
#endif
}

void hide_cursor() {
#ifndef YATAIDON_USE_SDL2
    SDL_HideCursor();
#else
    SDL_ShowCursor(SDL_DISABLE);
#endif
}
