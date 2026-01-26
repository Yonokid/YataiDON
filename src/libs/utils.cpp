#include <raylib.h>

#include "config.h"
#include "global_data.h"
#include "texture.h"

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons) {
    if (global_data.input_locked) return false;

    for (int key : keys) {
        if (IsKeyPressed(key)) return true;
    }
    if (IsGamepadAvailable(0)) {
        for (int button: gamepad_buttons) {
            if (IsGamepadButtonPressed(0, button)) return true;
        }
    }
    return false;
}

bool is_l_don_pressed(PlayerNum player_num = PlayerNum::ALL) {
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

bool is_r_don_pressed(PlayerNum player_num = PlayerNum::ALL) {
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

bool is_l_kat_pressed(PlayerNum player_num = PlayerNum::ALL) {
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

bool is_r_kat_pressed(PlayerNum player_num = PlayerNum::ALL) {
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

TextureWrapper global_tex;
