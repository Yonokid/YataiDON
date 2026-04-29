#include "input.h"
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef __EMSCRIPTEN__
#include <SDL3/SDL.h>
std::atomic<bool> input_thread_running{true};
std::thread input_thread;

static std::unordered_map<SDL_JoystickID, SDL_Joystick*> sdl_joysticks;
// keyed by joy_id * 256 + button_index
static std::unordered_map<int64_t, bool>  sdl_prev_button;
// keyed by joy_id * 256 + axis_index
static std::unordered_map<int64_t, float> sdl_prev_axis;

static void refresh_sdl_joysticks() {
    static bool init_done = false;
    if (!init_done) {
        SDL_InitSubSystem(SDL_INIT_JOYSTICK);
        init_done = true;
    }

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (!ids) return;

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
#endif
std::mutex input_mutex;
std::vector<int> pressed_keys;
std::vector<int> released_keys;

std::unordered_map<int, bool> previous_key_states;
std::unordered_map<int, bool> previous_gamepad_states;

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

    if (!ray::IsWindowFocused()) return false;

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

    for (int gamepad = 0; gamepad < 4; gamepad++) {
        if (!ray::IsGamepadAvailable(gamepad)) continue;
        for (int btn = 1; btn <= 17; btn++) {
            int key = 10000 + btn;
            bool current_state  = ray::IsGamepadButtonDown(gamepad, btn);
            bool previous_state = previous_gamepad_states[key];
            if (current_state  && !previous_state) local_pressed.push_back(key);
            if (!current_state && previous_state)  local_released.push_back(key);
            previous_gamepad_states[key] = current_state;
        }

    }

#ifndef __EMSCRIPTEN__
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

#ifndef __EMSCRIPTEN__
void shutdown_sdl_joysticks() {
    for (auto& [id, joy] : sdl_joysticks) {
        SDL_CloseJoystick(joy);
    }
    sdl_joysticks.clear();
}
#endif
