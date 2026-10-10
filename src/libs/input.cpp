#include "input.h"
#include "profiler.h"
#include "animation.h"
#include "spdlog/spdlog.h"
#include "texture.h"
#include "time.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

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


#include <SDL3/SDL.h>
std::atomic<bool> input_thread_running{true};
std::thread input_thread;

static std::atomic<double> last_input_ms{0.0};

static std::unordered_map<SDL_JoystickID, SDL_Joystick*> sdl_joysticks;
// keyed by joy_id * 256 + button_index
static std::unordered_map<int64_t, bool>  sdl_prev_button;
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
        if (!current.count(it->first)) {
            SDL_CloseJoystick(it->second);
            it = sdl_joysticks.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < count; i++) {
        if (SDL_IsGamepad(ids[i])) continue; // already handled by raylib
        if (!sdl_joysticks.count(ids[i])) {
            SDL_Joystick* joy = SDL_OpenJoystick(ids[i]);
            if (joy) {
                sdl_joysticks[ids[i]] = joy;
                spdlog::info("Joystick fallback: opened '{}' (id {})",
                             SDL_GetJoystickName(joy), (int)ids[i]);
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

static std::array<bool, 349> previous_key_states{};
static std::array<std::array<bool, 18>, 4> previous_gamepad_states{};

// Gamepad/joystick buttons and axes are folded into the key space at these
// offsets (config stores the bare button number).
static constexpr int GAMEPAD_VKEY_BASE = 10000;
static constexpr int AXIS_VKEY_BASE    = 20000;
static std::atomic<int> last_gamepad_vkey{0};

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons, float* strength) {

    for (int key : keys) {
        if (check_key_pressed(key, strength)) return true;
    }

    // Check gamepad buttons (offset by 10000)
    if (gamepad_buttons.empty()) return false;

    for (int button : gamepad_buttons) {
        if (check_key_pressed(10000 + button, strength)) return true;
    }
    return false;
}

bool is_l_don_pressed(PlayerNum player_num, float* strength) {
    if (player_num == PlayerNum::P1) {
        return is_input_key_pressed(global_data.config->keys_1p.left_don, global_data.config->gamepad_1p.left_don, strength)
            || check_key_pressed(TOUCH_L_DON, strength);
    } else if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.left_don, global_data.config->gamepad_2p.left_don, strength);
    } else if (player_num != PlayerNum::ALL) {
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
        return is_input_key_pressed(global_data.config->keys_1p.right_don, global_data.config->gamepad_1p.right_don, strength)
            || check_key_pressed(TOUCH_R_DON, strength);
    } else if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.right_don, global_data.config->gamepad_2p.right_don, strength);
    } else if (player_num != PlayerNum::ALL) {
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

bool is_l_kat_pressed(PlayerNum player_num, float* strength) {
    if (player_num == PlayerNum::P1) {
        return is_input_key_pressed(global_data.config->keys_1p.left_kat, global_data.config->gamepad_1p.left_kat, strength)
            || check_key_pressed(TOUCH_L_KAT, strength);
    } else if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.left_kat, global_data.config->gamepad_2p.left_kat, strength);
    } else if (player_num != PlayerNum::ALL) {
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
        return is_input_key_pressed(global_data.config->keys_1p.right_kat, global_data.config->gamepad_1p.right_kat, strength)
            || check_key_pressed(TOUCH_R_KAT, strength);
    } else if (player_num == PlayerNum::P2) {
        return is_input_key_pressed(global_data.config->keys_2p.right_kat, global_data.config->gamepad_2p.right_kat, strength);
    } else if (player_num != PlayerNum::ALL) {
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

static int touch_quadrant_vkey(ray::Vector2 pos, int sw, int sh) {
    bool left = pos.x < sw / 2.0f;
    bool top  = pos.y < sh / 2.0f;
    if (top) return left ? TOUCH_L_KAT : TOUCH_R_KAT;

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
    if (in_drum) return left ? TOUCH_L_DON : TOUCH_R_DON;
    return left ? TOUCH_L_KAT : TOUCH_R_KAT;
}

static bool touch_watch_registered = false;

static int char_to_raylib_key(unsigned char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 32 && c <= 96) return c;
    return 0;
}

#if defined(__linux__) && !defined(__ANDROID__)
static bool handle_linux_text_input(SDL_Event* event) {
    if (is_input_locked()) return false;
    if (event->type != SDL_EVENT_TEXT_INPUT || !event->text.text) return false;

    const bool* key_state = SDL_GetKeyboardState(nullptr);
    std::lock_guard<std::mutex> lock(input_mutex);
    for (const char* p = event->text.text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        int key = char_to_raylib_key(c);
        if (!key) continue;
        SDL_Keycode keycode = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        SDL_Keymod mod = SDL_KMOD_NONE;
        SDL_Scancode sc = SDL_GetScancodeFromKey(keycode, &mod);
        if (sc != SDL_SCANCODE_UNKNOWN && key_state && key_state[sc]) continue;
        pressed_keys.emplace(key, 1.0f);
        released_keys.insert(key);
    }
    return true;
}
#endif

static bool SDLCALL touch_event_watch(void* /*userdata*/, SDL_Event* event) {
    if (is_input_locked()) return 1;

    #if defined(__linux__) && !defined(__ANDROID__)
    if (handle_linux_text_input(event)) {
        return 1;
    }
    #endif

    if (event->type == SDL_EVENT_KEY_DOWN &&
        event->key.scancode == SDL_SCANCODE_AC_BACK) {
        std::lock_guard<std::mutex> lock(input_mutex);
        pressed_keys.emplace(ray::KEY_ESCAPE, 1.0f);
        return 0;
    }
    if (event->type == SDL_EVENT_KEY_UP &&
        event->key.scancode == SDL_SCANCODE_AC_BACK) {
        std::lock_guard<std::mutex> lock(input_mutex);
        released_keys.insert(ray::KEY_ESCAPE);
        return 0;
    }

    if (event->type == SDL_EVENT_FINGER_DOWN) {
        if (!global_data.config || !global_data.config->general.touch_input) return 1;
        if (!touch_drum_enabled.load(std::memory_order_relaxed)) return 1;
        SDL_FingerID id = event->tfinger.fingerID;
        std::lock_guard<std::mutex> lock(input_mutex);
        if (!touch_id_to_vkey.count(id)) {
            int sw = ray::GetScreenWidth();
            int sh = ray::GetScreenHeight();
            ray::Vector2 pos = { event->tfinger.x * sw, event->tfinger.y * sh };
            int vkey = touch_quadrant_vkey(pos, sw, sh);
#ifdef YATAIDON_PLATFORM_IOS
            vkey = ios_handle_touch_navigation(vkey, event->tfinger.x, event->tfinger.y);
#endif
            touch_id_to_vkey[id] = vkey;
            touch_drum_pressed.store(true, std::memory_order_relaxed);
            last_input_ms.store(get_current_ms(), std::memory_order_relaxed);
            pressed_keys.emplace(vkey, 1.0f);
        }
    } else if (event->type == SDL_EVENT_FINGER_UP ||
               event->type == SDL_EVENT_FINGER_CANCELED) {
        SDL_FingerID id = event->tfinger.fingerID;
        std::lock_guard<std::mutex> lock(input_mutex);
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
    if (!touch_drum_enabled.load(std::memory_order_relaxed)) return false;
    if (!global_data.config || !global_data.config->general.touch_input) return false;

    auto* touch_drum_resize = static_cast<TextureResizeAnimation*>(global_tex.get_animation(66));
    if (!touch_drum_resize) return false;

    if (!touch_drum_resize->isStarted()) touch_drum_resize->start();
    if (touch_drum_pressed.exchange(false, std::memory_order_relaxed))
        touch_drum_resize->restart();
    touch_drum_resize->update(get_current_ms());
    const float scale = (float)touch_drum_resize->attribute;
    float y_fix = 0.0f;
    auto drum_it = global_tex.textures.find("overlay/touch_drum");
    if (drum_it != global_tex.textures.end())
        y_fix = drum_it->second->height * 0.5f * (1.0f - scale);
    global_tex.draw_texture(global_tex.get_texture("overlay/touch_drum"), {.scale=scale, .center=true, .y=y_fix, .fade=0.5f});
#ifdef YATAIDON_PLATFORM_IOS
    ios_draw_touch_navigation_labels(static_cast<float>(ray::GetScreenWidth()), static_cast<float>(ray::GetScreenHeight()));
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
    if (is_input_locked()) return;
    thread_local std::vector<int> local_pressed;
    thread_local std::vector<int> local_released;
    local_pressed.clear();
    local_released.clear();

#ifdef _WIN32
    for (int key = 32; key < 349; key++) {
        bool current_state  = win32_is_key_down_native(key);
        bool previous_state = previous_key_states[key];
        if (current_state  && !previous_state) local_pressed.push_back(key);
        if (!current_state && previous_state)  local_released.push_back(key);
        previous_key_states[key] = current_state;
    }
#else
    for (int key = 32; key < 349; key++) {
        bool current_state  = ray::IsKeyDown(key);
        bool previous_state = previous_key_states[key];
        if (current_state  && !previous_state) local_pressed.push_back(key);
        if (!current_state && previous_state)  local_released.push_back(key);
        previous_key_states[key] = current_state;
    }
#endif

    for (int gamepad = 0; gamepad < 4; gamepad++) {
        if (!ray::IsGamepadAvailable(gamepad)) continue;
        for (int btn = 1; btn <= 17; btn++) {
            int key = 10000 + btn;
            bool current_state  = ray::IsGamepadButtonDown(gamepad, btn);
            bool previous_state = previous_gamepad_states[gamepad][btn];
            if (current_state  && !previous_state) local_pressed.push_back(key);
            if (!current_state && previous_state)  local_released.push_back(key);
            previous_gamepad_states[gamepad][btn] = current_state;
        }

    }

    refresh_sdl_joysticks();
    constexpr float JOYSTICK_AXIS_THRESHOLD = 0.5f;

    for (auto& [joy_id, joy] : sdl_joysticks) {
        int num_buttons = SDL_GetNumJoystickButtons(joy);
        for (int btn = 0; btn < num_buttons && btn < 32; btn++) {
            int64_t state_key = (int64_t)joy_id * 256 + btn;
            // Use 1-indexed encoding to match raylib's gamepad button convention
            int vkey = 10000 + btn + 1;
            bool cur  = SDL_GetJoystickButton(joy, btn);
            bool prev = sdl_prev_button[state_key];
            if (cur  && !prev) local_pressed.push_back(vkey);
            if (!cur && prev)  local_released.push_back(vkey);
            sdl_prev_button[state_key] = cur;
        }

        int num_axes = SDL_GetNumJoystickAxes(joy);
        for (int axis = 0; axis < num_axes && axis < 8; axis++) {
            int64_t state_key = (int64_t)joy_id * 256 + axis;
            float value = SDL_GetJoystickAxis(joy, axis) / 32767.0f;
            float prev  = sdl_prev_axis[state_key];
            int key_pos = 20000 + axis * 2;
            int key_neg = 20000 + axis * 2 + 1;
            bool cur_pos  = value >  JOYSTICK_AXIS_THRESHOLD;
            bool cur_neg  = value < -JOYSTICK_AXIS_THRESHOLD;
            bool prev_pos = prev  >  JOYSTICK_AXIS_THRESHOLD;
            bool prev_neg = prev  < -JOYSTICK_AXIS_THRESHOLD;
            if (cur_pos  && !prev_pos) local_pressed.push_back(key_pos);
            if (!cur_pos && prev_pos)  local_released.push_back(key_pos);
            if (cur_neg  && !prev_neg) local_pressed.push_back(key_neg);
            if (!cur_neg && prev_neg)  local_released.push_back(key_neg);
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

    if (!local_pressed.empty()) last_input_ms.store(get_current_ms(), std::memory_order_relaxed);

    if (!local_pressed.empty() || !local_released.empty()) {
        std::lock_guard<std::mutex> lock(input_mutex);
        for (int key : local_pressed) pressed_keys.emplace(key, 1.0f);
        released_keys.insert(local_released.begin(), local_released.end());
    }
}

double get_last_input_ms() {
    return last_input_ms.load(std::memory_order_relaxed);
}

int take_gamepad_button_pressed() {
    int vkey = last_gamepad_vkey.exchange(0, std::memory_order_relaxed);
    if (vkey < GAMEPAD_VKEY_BASE || vkey >= AXIS_VKEY_BASE) return -1;
    return vkey - GAMEPAD_VKEY_BASE;
}

void submit_gamepad_button_press(int button, float strength) {
    if (is_input_locked()) return;

    // The SDL joystick fallback currently exposes at most 32 buttons.
    if (button < 1 || button > 32) return;

    const int vkey = GAMEPAD_VKEY_BASE + button;
    last_gamepad_vkey.store(vkey, std::memory_order_relaxed);
    last_input_ms.store(get_current_ms(), std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(input_mutex);
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
        if (winrt_initialized) RoUninitialize();
    }
#endif
};

static std::unique_ptr<MidiInput> midi_input;

#ifdef _WIN32
static constexpr auto midi_api = libremidi::API::WINDOWS_UWP;
#else
static constexpr auto midi_api = libremidi::midi1::default_api();
#endif

static void midi_message_received(const libremidi::message& message, const MidiInput& state) {
    const auto& bytes = message.bytes;
    if (bytes.size() != 3 || (bytes[0] & 0xf0) != 0x90 || bytes[2] == 0) return;

    const int channel = (bytes[0] & 0x0f) + 1;
    if (state.channel != 0 && state.channel != channel) return;

    const auto mapping = state.note_to_button.find(bytes[1]);
    if (mapping != state.note_to_button.end()) {
        constexpr float minimum_strength = 0.5f;
        constexpr float maximum_strength = 1.15f;
        constexpr int normal_velocity = 80;
        const float strength = bytes[2] <= normal_velocity
            ? minimum_strength + (1.0f - minimum_strength) * (bytes[2] - 1) / (normal_velocity - 1)
            : 1.0f + (maximum_strength - 1.0f) * (bytes[2] - normal_velocity) / (127 - normal_velocity);

        submit_gamepad_button_press(mapping->second, strength);
    }
}

// Device notifications can arrive on background threads; open and close ports on the main thread.
static void queue_midi_port_event(MidiInput& state, const libremidi::input_port& port, bool added) {
    std::lock_guard<std::mutex> lock(state.events_mutex);
    state.events.push_back({port, added});
    state.events_pending.store(true, std::memory_order_release);
}

}
#endif

void start_midi_input(const MidiConfig& config) {
    if (config.device.empty()) return;
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
    if (result == RPC_E_CHANGED_MODE) result = RoInitialize(RO_INIT_SINGLETHREADED);

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

    state->observer = std::make_unique<libremidi::observer>(
        observer_config, libremidi::observer_configuration_for(midi_api));

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
    if (!midi_input || !midi_input->events_pending.exchange(false, std::memory_order_acquire)) return;
    auto& state = *midi_input;
    std::vector<MidiPortEvent> events;

    {
        std::lock_guard<std::mutex> lock(state.events_mutex);
        events.swap(state.events);
    }

    for (const auto& event : events) {
        if (!event.added) {
            if (state.selected && state.selected->api == event.port.api &&
                state.selected->port == event.port.port && state.selected->port_name == event.port.port_name) {
                state.port.reset();
                state.selected.reset();
                spdlog::info("MIDI input disconnected from '{}'", event.port.display_name);
            }

            continue;
        }

        spdlog::info("MIDI input available: {}", event.port.display_name);

        if (state.port || (event.port.display_name.find(state.device) == std::string::npos &&
                           event.port.device_name.find(state.device) == std::string::npos)) continue;

        libremidi::input_configuration input_config;

        input_config.on_message = [&state](const libremidi::message& message) {
            midi_message_received(message, state);
        };

        input_config.on_error = [](std::string_view error, const libremidi::source_location&) {
            spdlog::warn("MIDI input: {}", error);
        };

        auto port = std::make_unique<libremidi::midi_in>(
            input_config, libremidi::midi_in_configuration_for(midi_api));

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
    std::lock_guard<std::mutex> lock(input_mutex);
    auto it = pressed_keys.find(key);
    if (it != pressed_keys.end()) {
        if (strength) *strength = it->second;
        pressed_keys.erase(it);
        return true;
    }
    return false;
}

bool check_key_released(int key) {
    std::lock_guard<std::mutex> lock(input_mutex);
    auto it = released_keys.find(key);
    if (it != released_keys.end()) {
        released_keys.erase(it);
        return true;
    }
    return false;
}

void clear_input_buffers() {
    std::lock_guard<std::mutex> lock(input_mutex);
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
    sdl_prev_button.clear();
    sdl_prev_axis.clear();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    sdl_joysticks_init_done = false;
}

static bool keyboard_editing = false;

bool is_text_input_key(int key) {
    if (!keyboard_editing) return false;

    return (key >= ray::KEY_SPACE && key <= ray::KEY_GRAVE) ||
           (key >= ray::KEY_KP_0 && key <= ray::KEY_KP_EQUAL) ||
           (key >= ray::KEY_ESCAPE && key <= ray::KEY_END) ||
           (key >= ray::KEY_LEFT_SHIFT && key <= ray::KEY_RIGHT_SUPER);
}

void set_keyboard_visible(bool visible) {
    keyboard_editing = visible;
    #if defined(PLATFORM_ANDROID) || defined(YATAIDON_PLATFORM_IOS)
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        if (!windows || count == 0) return;
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
// raylib's SDL platform starts text input with the window (for GetCharPressed). With an IME
// such as a Chinese one, every key press then goes through the IME, and PollInputEvents took
// 5-150 ms on the frame after a drum hit on a keyboard-type controller. Text input is only on
// while poll_text_edit is in use.
static bool text_input_wanted = false;
static bool text_input_active = true;   // raylib started it

void sync_text_input() {
#if defined(PLATFORM_DESKTOP_SDL)
    if (text_input_wanted != text_input_active) {
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        if (windows && count > 0) {
            if (text_input_wanted) SDL_StartTextInput(windows[0]);
            else                   SDL_StopTextInput(windows[0]);
            text_input_active = text_input_wanted;
        }
        SDL_free(windows);
    }
#endif
    text_input_wanted = false;
}

TextEditAction poll_text_edit(std::string& text, const std::function<bool(int)>& accept) {
    text_input_wanted = true;
    // Ctrl+V pastes the clipboard (first line only, UTF-8 as raylib hands it over).
    if ((ray::IsKeyDown(ray::KEY_LEFT_CONTROL) || ray::IsKeyDown(ray::KEY_RIGHT_CONTROL)) &&
        ray::IsKeyPressed(ray::KEY_V)) {
        const char* clip = ray::GetClipboardText();
        if (clip) {
            std::string s(clip);
            const size_t eol = s.find_first_of("\r\n");
            if (eol != std::string::npos) s.resize(eol);
            text += s;
        }
        while (ray::GetCharPressed() > 0) {}   // the 'v' itself is not typed
        return TextEditAction::None;
    }

    TextEditAction action = TextEditAction::None;
    if (ray::IsKeyPressed(ray::KEY_BACKSPACE)) {
        // Remove one whole UTF-8 code point (continuation bytes first).
        while (!text.empty() && ((unsigned char)text.back() & 0xC0) == 0x80)
            text.pop_back();
        if (!text.empty()) text.pop_back();
    } else if (ray::IsKeyPressed(ray::KEY_ESCAPE)) {
        action = TextEditAction::Cancel;
    } else if (ray::IsKeyPressed(ray::KEY_ENTER)) {
        action = TextEditAction::Confirm;
    }

    int key = ray::GetCharPressed();
    while (key > 0) {
        if (key == '\n' || key == '\r') {
            if (action == TextEditAction::None) action = TextEditAction::Confirm;
        } else if (key >= 0x20 && (!accept || accept(key))) {
            // GetCharPressed yields a Unicode code point (IME-composed CJK
            // included); store it as UTF-8.
            int n = 0;
            const char* utf8 = ray::CodepointToUTF8(key, &n);
            text.append(utf8, n);
        }
        key = ray::GetCharPressed();
    }
    return action;
}
