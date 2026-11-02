#pragma once

#include <algorithm>
#include <array>
#include <cmath>

inline std::array<float, 3> sprite_rgb_to_hsv(const std::array<float, 4>& color) {
    const float r = color[0], g = color[1], b = color[2];
    const std::array<float, 4> p = g >= b ? std::array{g, b, 0.0f, -1.0f / 3.0f} : std::array{b, g, -1.0f, 2.0f / 3.0f};
    const std::array<float, 4> q = r >= p[0] ? std::array{r, p[1], p[2], p[0]} : std::array{p[0], p[1], p[3], r};
    const float d = q[0] - std::min(q[3], q[1]);
    constexpr float epsilon = 1.0e-10f;
    return {std::abs(q[2] + (q[3] - q[1]) / (6.0f * d + epsilon)), d / (q[0] + epsilon), q[0]};
}

inline std::array<float, 4> sprite_color_transform(const std::array<float, 4>& source,
                                                   const std::array<float, 4>& target) {
    const auto from = sprite_rgb_to_hsv(source), to = sprite_rgb_to_hsv(target);
    return {to[0] - from[0], from[1] > 0 ? to[1] / from[1] : 1.0f, from[2] > 0 ? to[2] / from[2] : 1.0f, 1.0f};
}
