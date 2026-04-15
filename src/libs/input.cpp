#include "input.h"
#include <unordered_map>
#include <cstring>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif
#ifndef __EMSCRIPTEN__
#include <SDL3/SDL.h>
#endif

#ifndef __EMSCRIPTEN__
std::atomic<bool> input_thread_running{true};
std::thread input_thread;
#endif
std::mutex input_mutex;
std::vector<int> pressed_keys;
std::vector<int> released_keys;

std::unordered_map<int, bool> previous_key_states;

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons) {
    if (global_data.input_locked) return false;

    for (int key : keys) {
        if (check_key_pressed(key)) return true;
    }

    // Check gamepad buttons (offset by 10000)
    if (gamepad_buttons.empty()) return false;

    for (int button : gamepad_buttons) {
        if (check_key_pressed(10000 + button)) return true;
    }
    return false;
}

bool is_l_don_pressed(PlayerNum player_num) {
    if (global_data.input_locked) return false;
    std::vector<int> keys;
    if (player_num == PlayerNum::ALL) {
        keys = global_data.config->keys_1p.left_don;
        std::vector<int> keys_2p = global_data.config->keys_2p.left_don;
        keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    } else if (player_num == PlayerNum::P1) {
        keys = global_data.config->keys_1p.left_don;
    } else if (player_num == PlayerNum::P2) {
        keys = global_data.config->keys_2p.left_don;
    } else {
        return false;
    }

    std::vector<int> gamepad_buttons = global_data.config->gamepad.left_don;
    return is_input_key_pressed(keys, gamepad_buttons);
}

bool is_r_don_pressed(PlayerNum player_num) {
    if (global_data.input_locked) return false;
    std::vector<int> keys;
    if (player_num == PlayerNum::ALL) {
        keys = global_data.config->keys_1p.right_don;
        std::vector<int> keys_2p = global_data.config->keys_2p.right_don;
        keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    } else if (player_num == PlayerNum::P1) {
        keys = global_data.config->keys_1p.right_don;
    } else if (player_num == PlayerNum::P2) {
        keys = global_data.config->keys_2p.right_don;
    } else {
        return false;
    }

    std::vector<int> gamepad_buttons = global_data.config->gamepad.right_don;
    return is_input_key_pressed(keys, gamepad_buttons);
}

bool is_l_kat_pressed(PlayerNum player_num) {
    if (global_data.input_locked) return false;
    std::vector<int> keys;
    if (player_num == PlayerNum::ALL) {
        keys = global_data.config->keys_1p.left_kat;
        std::vector<int> keys_2p = global_data.config->keys_2p.left_kat;
        keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    } else if (player_num == PlayerNum::P1) {
        keys = global_data.config->keys_1p.left_kat;
    } else if (player_num == PlayerNum::P2) {
        keys = global_data.config->keys_2p.left_kat;
    } else {
        return false;
    }

    std::vector<int> gamepad_buttons = global_data.config->gamepad.left_kat;
    return is_input_key_pressed(keys, gamepad_buttons);
}

bool is_r_kat_pressed(PlayerNum player_num) {
    if (global_data.input_locked) return false;
    std::vector<int> keys;
    if (player_num == PlayerNum::ALL) {
        keys = global_data.config->keys_1p.right_kat;
        std::vector<int> keys_2p = global_data.config->keys_2p.right_kat;
        keys.insert(keys.end(), keys_2p.begin(), keys_2p.end());
    } else if (player_num == PlayerNum::P1) {
        keys = global_data.config->keys_1p.right_kat;
    } else if (player_num == PlayerNum::P2) {
        keys = global_data.config->keys_2p.right_kat;
    } else {
        return false;
    }

    std::vector<int> gamepad_buttons = global_data.config->gamepad.right_kat;
    return is_input_key_pressed(keys, gamepad_buttons);
}

#ifdef _WIN32
// Windows-specific key state checking using GetAsyncKeyState
bool is_key_down_native(int raylib_key) {
    int vk_code = 0;

    // Handle letters (A-Z)
    if (raylib_key >= 65 && raylib_key <= 90) {
        vk_code = raylib_key;
    }
    // Handle numbers (0-9)
    else if (raylib_key >= 48 && raylib_key <= 57) {
        vk_code = raylib_key;
    }
    // Handle special keys
    else {
        switch (raylib_key) {
            case 32: vk_code = VK_SPACE; break;
            case 256: vk_code = VK_ESCAPE; break;
            case 257: vk_code = VK_RETURN; break;
            case 258: vk_code = VK_TAB; break;
            case 259: vk_code = VK_BACK; break;
            case 260: vk_code = VK_INSERT; break;
            case 261: vk_code = VK_DELETE; break;
            case 262: vk_code = VK_RIGHT; break;
            case 263: vk_code = VK_LEFT; break;
            case 264: vk_code = VK_DOWN; break;
            case 265: vk_code = VK_UP; break;
            case 266: vk_code = VK_PRIOR; break; // Page Up
            case 267: vk_code = VK_NEXT; break;  // Page Down
            case 268: vk_code = VK_HOME; break;
            case 269: vk_code = VK_END; break;
            case 280: vk_code = VK_CAPITAL; break;
            case 281: vk_code = VK_SCROLL; break;
            case 282: vk_code = VK_NUMLOCK; break;
            case 283: vk_code = VK_SNAPSHOT; break; // Print Screen
            case 284: vk_code = VK_PAUSE; break;
            case 290: vk_code = VK_F1; break;
            case 291: vk_code = VK_F2; break;
            case 292: vk_code = VK_F3; break;
            case 293: vk_code = VK_F4; break;
            case 294: vk_code = VK_F5; break;
            case 295: vk_code = VK_F6; break;
            case 296: vk_code = VK_F7; break;
            case 297: vk_code = VK_F8; break;
            case 298: vk_code = VK_F9; break;
            case 299: vk_code = VK_F10; break;
            case 300: vk_code = VK_F11; break;
            case 301: vk_code = VK_F12; break;
            case 340: vk_code = VK_LSHIFT; break;
            case 341: vk_code = VK_LCONTROL; break;
            case 342: vk_code = VK_LMENU; break; // Left Alt
            case 343: vk_code = VK_LWIN; break;
            case 344: vk_code = VK_RSHIFT; break;
            case 345: vk_code = VK_RCONTROL; break;
            case 346: vk_code = VK_RMENU; break; // Right Alt
            case 347: vk_code = VK_RWIN; break;
            default: return false;
        }
    }

    if (vk_code == 0) return false;

    return (GetAsyncKeyState(vk_code) & 0x8000) != 0;
}
#endif

// Scan all keyboard keys once and push press/release events.
// Used by the polling thread on desktop and called directly per-frame on web.
void poll_keyboard_once() {
    static std::vector<int> local_pressed;
    static std::vector<int> local_released;
    local_pressed.clear();
    local_released.clear();

#ifdef _WIN32
    for (int key = 32; key < 349; key++) {
        bool current_state  = is_key_down_native(key);
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

    if (!local_pressed.empty() || !local_released.empty()) {
        std::lock_guard<std::mutex> lock(input_mutex);
        pressed_keys.insert(pressed_keys.end(), local_pressed.begin(), local_pressed.end());
        released_keys.insert(released_keys.end(), local_released.begin(), local_released.end());
    }
}

#ifndef __EMSCRIPTEN__
void input_polling_thread() {
    while (input_thread_running) {
        poll_keyboard_once();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}
#endif

#ifndef __EMSCRIPTEN__

static SDL_Joystick*  sdl_joysticks[4]    = {};
static SDL_JoystickID sdl_joystick_ids[4] = {};
static Sint16 sdl_axis_prev[4][16] = {};
static Uint8  sdl_hat_prev[4][8]   = {};
static const Sint16 AXIS_THRESHOLD = 3276;

// Virtual button offsets (added to 10000; config values are the raw offsets):
//   button_index                  physical button
//   200 + axis*2                  axis positive
//   201 + axis*2                  axis negative
//   400 + hat*4 + bit             hat direction (bit: 0=UP 1=RIGHT 2=DOWN 3=LEFT)

static int find_joystick_slot(SDL_JoystickID id) {
    for (int i = 0; i < 4; i++)
        if (sdl_joysticks[i] && sdl_joystick_ids[i] == id) return i;
    return -1;
}

static int find_free_slot() {
    for (int i = 0; i < 4; i++)
        if (!sdl_joysticks[i]) return i;
    return -1;
}

static void open_joystick_slot(int slot, SDL_JoystickID id) {
    sdl_joysticks[slot]    = SDL_OpenJoystick(id);
    sdl_joystick_ids[slot] = id;
    memset(sdl_axis_prev[slot], 0, sizeof(sdl_axis_prev[slot]));
    memset(sdl_hat_prev[slot],  0, sizeof(sdl_hat_prev[slot]));
    if (sdl_joysticks[slot]) {
        spdlog::info("Joystick connected: {}", SDL_GetJoystickName(sdl_joysticks[slot]));
    } else {
        spdlog::error("Failed to open joystick id={}: {}", id, SDL_GetError());
    }
}

static void handle_joystick_event(SDL_Event* event) {
    switch (event->type) {

    case SDL_EVENT_JOYSTICK_ADDED: {
        SDL_JoystickID id = event->jdevice.which;
        if (find_joystick_slot(id) == -1) {
            int slot = find_free_slot();
            if (slot >= 0) open_joystick_slot(slot, id);
        }
        break;
    }

    case SDL_EVENT_JOYSTICK_REMOVED: {
        int slot = find_joystick_slot(event->jdevice.which);
        if (slot >= 0) {
            spdlog::info("Joystick disconnected");
            SDL_CloseJoystick(sdl_joysticks[slot]);
            sdl_joysticks[slot]    = nullptr;
            sdl_joystick_ids[slot] = 0;
        }
        break;
    }

    case SDL_EVENT_JOYSTICK_BUTTON_DOWN: {
        std::lock_guard<std::mutex> lock(input_mutex);
        pressed_keys.push_back(10000 + event->jbutton.button);
        break;
    }

    case SDL_EVENT_JOYSTICK_BUTTON_UP: {
        std::lock_guard<std::mutex> lock(input_mutex);
        released_keys.push_back(10000 + event->jbutton.button);
        break;
    }

    case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
        int slot = find_joystick_slot(event->jaxis.which);
        if (slot < 0 || event->jaxis.axis >= 16) break;
        int    ax   = event->jaxis.axis;
        Sint16 val  = event->jaxis.value;
        Sint16 prev = sdl_axis_prev[slot][ax];
        sdl_axis_prev[slot][ax] = val;
        bool pos_cur  = val  >  AXIS_THRESHOLD;
        bool pos_prev = prev >  AXIS_THRESHOLD;
        bool neg_cur  = val  < -AXIS_THRESHOLD;
        bool neg_prev = prev < -AXIS_THRESHOLD;
        std::lock_guard<std::mutex> lock(input_mutex);
        if (pos_cur  && !pos_prev) pressed_keys.push_back(10200 + ax * 2);
        if (!pos_cur && pos_prev)  released_keys.push_back(10200 + ax * 2);
        if (neg_cur  && !neg_prev) pressed_keys.push_back(10200 + ax * 2 + 1);
        if (!neg_cur && neg_prev)  released_keys.push_back(10200 + ax * 2 + 1);
        break;
    }

    case SDL_EVENT_JOYSTICK_HAT_MOTION: {
        int slot = find_joystick_slot(event->jhat.which);
        if (slot < 0 || event->jhat.hat >= 8) break;
        int   h    = event->jhat.hat;
        Uint8 cur  = event->jhat.value;
        Uint8 prev = sdl_hat_prev[slot][h];
        sdl_hat_prev[slot][h] = cur;
        std::lock_guard<std::mutex> lock(input_mutex);
        for (int bit = 0; bit < 4; bit++) {
            Uint8 mask     = static_cast<Uint8>(1 << bit);
            bool  cur_bit  = (cur  & mask) != 0;
            bool  prev_bit = (prev & mask) != 0;
            int   vkey     = 10400 + h * 4 + bit;
            if (cur_bit  && !prev_bit) pressed_keys.push_back(vkey);
            if (!cur_bit && prev_bit)  released_keys.push_back(vkey);
        }
        break;
    }

    }
}

void init_sdl_gamepads() {
    if (!(SDL_WasInit(0) & SDL_INIT_JOYSTICK)) {
        if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
            spdlog::error("SDL3: joystick subsystem init failed: {}", SDL_GetError());
        }
    }

    SDL_SetEventEnabled(SDL_EVENT_GAMEPAD_ADDED,   false);
    SDL_SetEventEnabled(SDL_EVENT_GAMEPAD_REMOVED, false);

    SDL_PumpEvents();
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (ids) {
        for (int i = 0; i < count && i < 4; i++)
            open_joystick_slot(i, ids[i]);
        SDL_free(ids);
    }
}

void poll_sdl_gamepads() {
    SDL_PumpEvents();
    SDL_Event events[64];
    int n = SDL_PeepEvents(events, 64, SDL_GETEVENT,
                           SDL_EVENT_JOYSTICK_AXIS_MOTION,
                           SDL_EVENT_JOYSTICK_UPDATE_COMPLETE);
    for (int i = 0; i < n; i++)
        handle_joystick_event(&events[i]);
}

#endif  // __EMSCRIPTEN__

int get_any_controller_pressed() {
    std::lock_guard<std::mutex> lock(input_mutex);
    for (auto it = pressed_keys.begin(); it != pressed_keys.end(); ++it) {
        if (*it >= 10000) {
            int config_value = *it - 10000;
            pressed_keys.erase(it);
            return config_value;
        }
    }
    return -1;
}

bool check_key_pressed(int key) {
    std::lock_guard<std::mutex> lock(input_mutex);
    auto it = std::find(pressed_keys.begin(), pressed_keys.end(), key);
    if (it != pressed_keys.end()) {
        pressed_keys.erase(it);
        return true;
    }
    return false;
}

bool check_key_released(int key) {
    std::lock_guard<std::mutex> lock(input_mutex);
    auto it = std::find(released_keys.begin(), released_keys.end(), key);
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
