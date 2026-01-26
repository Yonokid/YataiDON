#pragma once

#include "global_data.h"
#include "texture.h"

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons);
bool is_l_don_pressed(PlayerNum player_num = PlayerNum::ALL);
bool is_r_don_pressed(PlayerNum player_num = PlayerNum::ALL);
bool is_l_kat_pressed(PlayerNum player_num = PlayerNum::ALL);
bool is_r_kat_pressed(PlayerNum player_num = PlayerNum::ALL);

extern TextureWrapper global_tex;
