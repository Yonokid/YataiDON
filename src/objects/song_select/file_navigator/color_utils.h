#pragma once

#include "../../enums.h"
#include "../../libs/ray.h"
#include <optional>

float rgb_to_hue(int r, int g, int b);
float calculate_hue_shift(const ray::Color& source_rgb, const ray::Color& target_rgb);
ray::Color darken_color(const ray::Color& rgb);
ray::Color parse_hex_color(const std::string& color);

extern const std::map<GenreIndex, std::array<std::optional<ray::Color>, 2>> DEFAULT_COLORS;
