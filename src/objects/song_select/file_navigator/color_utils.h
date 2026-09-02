#pragma once

#include "../../enums.h"
#include "../../../libs/color_utils.h"
#include <optional>

float rgb_to_hue(int r, int g, int b);
float calculate_hue_shift(const ray::Color& source_rgb, const ray::Color& target_rgb);
ray::Color darken_color(const ray::Color& rgb);

struct BoxColors {
    std::optional<ray::Color> box;
    ray::Color outline;
    ray::Color text;
};

BoxColors resolve_box_colors(const std::optional<ray::Color>& box_color,
                              const std::optional<ray::Color>& back_color,
                              const std::optional<ray::Color>& fore_color);

extern const std::map<GenreIndex, std::array<std::optional<ray::Color>, 2>> DEFAULT_COLORS;
