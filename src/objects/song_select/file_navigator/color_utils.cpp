#include "color_utils.h"
#include <algorithm>
#include <math.h>
#include <stdexcept>

float rgb_to_hue(int r, int g, int b) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max_val = std::max({rf, gf, bf});
    float min_val = std::min({rf, gf, bf});
    float delta = max_val - min_val;

    if (delta == 0.0f) return 0.0f;

    float hue = 0.0f;
    if (max_val == rf) {
        hue = 60.0f * (std::fmod((gf - bf) / delta, 6.0f));
    } else if (max_val == gf) {
        hue = 60.0f * ((bf - rf) / delta + 2.0f);
    } else {
        hue = 60.0f * ((rf - gf) / delta + 4.0f);
    }

    if (hue < 0.0f) hue += 360.0f;
    return hue;
}

float calculate_hue_shift(const ray::Color source_rgb, const ray::Color target_rgb) {
    float source_hue = rgb_to_hue(source_rgb.r, source_rgb.g, source_rgb.b);
    float target_hue = rgb_to_hue(target_rgb.r, target_rgb.g, target_rgb.b);

    float shift = (target_hue - source_hue) / 360.0f;
    while (shift <  0.0f) shift += 1.0f;
    while (shift >= 1.0f) shift -= 1.0f;
    return shift;
}

ray::Color darken_color(const ray::Color& rgb) {
    constexpr float darkening_factor = 0.63f;
    return ray::Color(
        (rgb.r * darkening_factor),
        (rgb.g * darkening_factor),
        (rgb.b * darkening_factor),
        255
    );
}

ray::Color parse_hex_color(const std::string& input) {
    std::string color = input;
    if (!color.empty() && color[0] == '#')
        color = color.substr(1);
    if (color.size() == 3) {
        color = std::string{color[0], color[0], color[1], color[1], color[2], color[2]};
    }
    if (color.size() != 6)
        throw std::invalid_argument("Invalid hex color: " + input);
    return ray::Color(
        std::stoi(color.substr(0, 2), nullptr, 16),
        std::stoi(color.substr(2, 2), nullptr, 16),
        std::stoi(color.substr(4, 2), nullptr, 16),
        255
    );
}

const std::map<GenreIndex, std::array<std::optional<ray::Color>, 2>> DEFAULT_COLORS = {
    { GenreIndex::JPOP,        { ray::Color(32,  160, 186), ray::Color(0,   77,  104) } },
    { GenreIndex::ANIME,       { ray::Color(255, 152, 0),   ray::Color(156, 64,  2)   } },
    { GenreIndex::VOCALOID,    { std::nullopt,        ray::Color(84,  101, 126) } },
    { GenreIndex::CHILDREN,    { ray::Color(255, 82,  134), ray::Color(153, 4,   46)  } },
    { GenreIndex::VARIETY,     { ray::Color(142, 212, 30),  ray::Color(60,  104, 0)   } },
    { GenreIndex::CLASSICAL,   { ray::Color(209, 162, 19),  ray::Color(134, 88,  0)   } },
    { GenreIndex::GAME,        { ray::Color(156, 117, 189), ray::Color(79,  40,  134) } },
    { GenreIndex::NAMCO,       { ray::Color(255, 90,  19),  ray::Color(148, 24,  0)   } },
    { GenreIndex::DEFAULT,     { std::nullopt,        ray::Color(101, 0,   82)  } },
    { GenreIndex::RECOMMENDED, { std::nullopt,        ray::Color(140, 39,  92)  } },
    { GenreIndex::FAVORITE,    { std::nullopt,        ray::Color(151, 57,  30)  } },
    { GenreIndex::RECENT,      { std::nullopt,        ray::Color(35,  123, 103) } },
    { GenreIndex::DAN,         { ray::Color(35,  102, 170), ray::Color(25,  68,  137) } },
    { GenreIndex::DIFFICULTY,  { ray::Color(255, 85,  95),  ray::Color(157, 13,  31)  } },
};
