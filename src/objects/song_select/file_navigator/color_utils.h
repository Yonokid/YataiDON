#pragma once

#include "../../enums.h"
#include "../../libs/color_utils.h"
#include <optional>

float rgb_to_hue(int r, int g, int b);
float calculate_hue_shift(const ray::Color& source_rgb, const ray::Color& target_rgb);
ray::Color darken_color(const ray::Color& rgb);

extern const std::map<GenreIndex, std::array<std::optional<ray::Color>, 2>> DEFAULT_COLORS;
