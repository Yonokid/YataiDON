#pragma once

#include <array>
#include <map>
#include <optional>
#include <vector>

#include "../../../libs/gfx/color_utils.h"
#include "../../enums.h"

float rgb_to_hue(int r, int g, int b);
float calculate_hue_shift(const Color& source_rgb, const Color& target_rgb);
Color darken_color(const Color& rgb);

struct BoxColors {
    std::vector<Color> box;
    Color outline;
    Color text;
};

BoxColors resolve_box_colors(const std::vector<Color>& box_color, const std::vector<Color>& back_color,
                             const std::optional<Color>& fore_color);

extern const std::map<GenreIndex, std::array<std::optional<Color>, 2>> DEFAULT_COLORS;
