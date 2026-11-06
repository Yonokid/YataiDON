#pragma once
#include "asset_cache.h"
#include "gfx.h"

// Up to 64 colours (one "byte" each) on every platform: compiling a shader per request is waste.
inline AssetCache<Shader>& color_shader_cache() {
    static AssetCache<Shader> cache(64);
    return cache;
}

inline std::shared_ptr<Shader> load_color_shader(Color color, Color from = {142, 212, 30, 255}) {
    auto rgb = [](Color c) {
        return std::to_string((static_cast<unsigned>(c.r) << 16) | (static_cast<unsigned>(c.g) << 8) | c.b);
    };
    const std::string key = rgb(from) + ">" + rgb(color);

    if (auto shader = color_shader_cache().get(key)) {
        return shader;
    }
    auto shader = std::shared_ptr<Shader>(new Shader(load_shader("shader/dummy.vs", "shader/colortransform.fs")),
                                          [](Shader* shader) {
                                              unload_shader(*shader);
                                              delete shader;
                                          });
    float source[3] = {from.r / 255.0f, from.g / 255.0f, from.b / 255.0f};
    float target[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    set_shader_value(*shader, get_shader_location(*shader, "sourceColor"), source, shader_uniform::VEC3);
    set_shader_value(*shader, get_shader_location(*shader, "targetColor"), target, shader_uniform::VEC3);
    color_shader_cache().put(key, shader, 1);
    return shader;
}
