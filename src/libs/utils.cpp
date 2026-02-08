#include "utils.h"

std::atomic<bool> input_thread_running{true};
std::thread input_thread;
std::mutex input_mutex;
std::vector<int> pressed_keys;
std::vector<int> released_keys;

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons) {
    if (global_data.input_locked) return false;

    for (int key : keys) {
        if (check_key_pressed(key)) return true;
    }

    // Check gamepad buttons (offset by 10000)
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

void input_polling_thread() {
    std::vector<int> local_pressed;
    std::vector<int> local_released;

    while (input_thread_running) {
        {
        ray::PollInputEvents();

        local_pressed.clear();
        local_released.clear();

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

        if (!local_pressed.empty() || !local_released.empty()) {
            std::lock_guard<std::mutex> lock(input_mutex);
            pressed_keys.insert(pressed_keys.end(), local_pressed.begin(), local_pressed.end());
            released_keys.insert(released_keys.end(), local_released.begin(), local_released.end());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
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
