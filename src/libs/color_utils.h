#pragma once

#include <stdexcept>
#include <string>
#include "ray.h"

inline ray::Color parse_hex_color(const std::string& input) {
    std::string color = input;
    if (!color.empty() && color[0] == '#')
        color = color.substr(1);
    if (color.size() == 3)
        color = std::string{color[0], color[0], color[1], color[1], color[2], color[2]};
    if (color.size() != 6)
        throw std::invalid_argument("Invalid hex color: " + input);
    return ray::Color(
        std::stoi(color.substr(0, 2), nullptr, 16),
        std::stoi(color.substr(2, 2), nullptr, 16),
        std::stoi(color.substr(4, 2), nullptr, 16),
        255
    );
}

inline std::string color_to_hex(ray::Color c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}
