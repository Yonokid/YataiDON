#include "utils.h"
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

std::atomic<bool> input_thread_running{true};
std::thread input_thread;
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

void input_polling_thread() {
    std::vector<int> local_pressed;
    std::vector<int> local_released;

    // Reserve capacity to avoid reallocations during hot path
    local_pressed.reserve(16);
    local_released.reserve(16);

    while (input_thread_running) {
        local_pressed.clear();
        local_released.clear();

#ifdef _WIN32
        // Windows: Use native GetAsyncKeyState (thread-safe)
        for (int key = 32; key < 349; key++) {
            bool current_state = is_key_down_native(key);
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
        ray::PollInputEvents();

        for (int key = 32; key < 349; key++) {
            if (ray::IsKeyPressed(key)) {
                local_pressed.push_back(key);
            }
            if (ray::IsKeyReleased(key)) {
                local_released.push_back(key);
            }
        }

        // Check gamepad buttons (use offset 10000 to differentiate from keyboard)
        if (ray::IsGamepadAvailable(0)) {
            for (int button = 0; button < 32; button++) {
                if (ray::IsGamepadButtonPressed(0, button)) {
                    local_pressed.push_back(10000 + button);
                }
                if (ray::IsGamepadButtonReleased(0, button)) {
                    local_released.push_back(10000 + button);
                }
            }
        }
#endif

        if (!local_pressed.empty() || !local_released.empty()) {
            std::lock_guard<std::mutex> lock(input_mutex);
            pressed_keys.insert(pressed_keys.end(), local_pressed.begin(), local_pressed.end());
            released_keys.insert(released_keys.end(), local_released.begin(), local_released.end());
        }

        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void poll_gamepad_events() {
#ifdef _WIN32
    std::vector<int> local_pressed;
    std::vector<int> local_released;

    if (ray::IsGamepadAvailable(0)) {
        for (int button = 0; button < 32; button++) {
            if (ray::IsGamepadButtonPressed(0, button)) {
                local_pressed.push_back(10000 + button);
            }
            if (ray::IsGamepadButtonReleased(0, button)) {
                local_released.push_back(10000 + button);
            }
        }
    }

    if (!local_pressed.empty() || !local_released.empty()) {
        std::lock_guard<std::mutex> lock(input_mutex);
        pressed_keys.insert(pressed_keys.end(), local_pressed.begin(), local_pressed.end());
        released_keys.insert(released_keys.end(), local_released.begin(), local_released.end());
    }
#endif
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

TextureWrapper global_tex;
