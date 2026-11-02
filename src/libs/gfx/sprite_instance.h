#pragma once

#include <array>
#include <cstdint>

#include "gfx_types.h"

struct alignas(64) SpriteInstance {
    std::array<float, 4> rectangle;
    std::array<float, 4> uv;
    std::array<float, 4> origin_rotation;
    std::array<float, 4> color_transform;

    static SpriteInstance make(Rect destination, std::array<float, 4> uv, Vec2 origin, float rotation, Color tint,
                               uint32_t texture_index, std::array<float, 4> transform) {
        // Integer-valued floats preserve every packed bit without relying on NaN payloads.
        const uint32_t rg =
            tint.r | (uint32_t{tint.g} << 8) | (texture_index << 16) | (uint32_t{transform[3] > 0.5f} << 19);
        transform[3] = static_cast<float>(tint.b | (uint32_t{tint.a} << 8));
        return {{destination.x, destination.y, destination.width, destination.height},
                uv,
                {origin.x, origin.y, rotation, static_cast<float>(rg)},
                transform};
    }
};

static_assert(sizeof(SpriteInstance) == 64);
