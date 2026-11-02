#pragma once
#include "asset_cache.h"
#include "gfx.h"

// Up to 64 colours (one "byte" each) on every platform: compiling a shader per request is waste.
inline AssetCache<Shader>& color_shader_cache() {
    static AssetCache<Shader> cache(64);
    return cache;
}

inline std::shared_ptr<Shader> load_color_shader(Color color) {
    const std::string key =
        std::to_string((static_cast<unsigned>(color.r) << 16) | (static_cast<unsigned>(color.g) << 8) | color.b);

    if (auto shader = color_shader_cache().get(key)) {
        return shader;
    }
    auto shader = std::shared_ptr<Shader>(new Shader(load_shader("shader/dummy.vs", "shader/colortransform.fs")),
                                          [](Shader* shader) {
                                              unload_shader(*shader);
                                              delete shader;
                                          });
    float source[3] = {142 / 255.0f, 212 / 255.0f, 30 / 255.0f};
    float target[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    set_shader_value(*shader, get_shader_location(*shader, "sourceColor"), source, shader_uniform::VEC3);
    set_shader_value(*shader, get_shader_location(*shader, "targetColor"), target, shader_uniform::VEC3);
    color_shader_cache().put(key, shader, 1);
    return shader;
}
