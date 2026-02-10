#include "script.h"

void ScriptManager::init(fs::path script_path) {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string,
                       sol::lib::math, sol::lib::table);

    for (const auto& script : fs::directory_iterator(script_path)) {
        scripts[script.path().stem().string()] = script.path();
    }
    spdlog::debug("Loaded scripts:");
    for (const auto& [name, path] : scripts) {
        spdlog::debug("  {} -> {}", name, path);
    }
    spdlog::debug("Total scripts: {}", scripts.size());

    tex.init(script_path.parent_path() / "Graphics");

    register_lua_bindings();
}

std::string ScriptManager::get_lua_script_path(const std::string& script_name) {
    if (scripts.find(script_name) == scripts.end()) {
        throw std::runtime_error("Unable to find lua script: " + script_name);
    }
    return scripts[script_name];
}

void ScriptManager::register_lua_bindings() {
    lua.new_usertype<BaseAnimation>("BaseAnimation",
        "update", &BaseAnimation::update,

        "attribute", &BaseAnimation::attribute
    );

    sol::table tex = lua.create_table();

    tex.set_function("load_animations", [](const std::string& screen_name) {
        script_manager.tex.load_animations(screen_name);
    });

    tex.set_function("get_animation", [](int anim_id, sol::optional<bool> is_copy) -> BaseAnimation* {
        bool copy = is_copy.value_or(false);
        return script_manager.tex.get_animation(anim_id, copy);
    });

    tex.set_function("load_folder", [](const std::string& screen_name, const std::string& subset) {
        script_manager.tex.load_folder(screen_name, subset);
    });

    tex.set_function("get_texture_info", [](const std::string& subset, const std::string& texture_name) -> sol::optional<sol::table> {
        auto subset_it = script_manager.tex.textures.find(subset);
        if (subset_it == script_manager.tex.textures.end()) {
            return sol::nullopt;
        }

        auto texture_it = subset_it->second.find(texture_name);
        if (texture_it == subset_it->second.end()) {
            return sol::nullopt;
        }

        const auto& tex_obj = texture_it->second;

        sol::table info = script_manager.lua.create_table();
        info["name"] = tex_obj->name;
        info["width"] = tex_obj->width;
        info["height"] = tex_obj->height;

        return info;
    });

    tex.set_function("draw_texture", [](const std::string& subset, const std::string& texture_name, sol::optional<sol::table> params_table) {
        DrawTextureParams params;

        if (params_table) {
            sol::table t = params_table.value();

            // color (as table {r, g, b, a})
            sol::optional<sol::table> color = t["color"];
            if (color) {
                params.color.r = color.value()[1].get_or(params.color.r);
                params.color.g = color.value()[2].get_or(params.color.g);
                params.color.b = color.value()[3].get_or(params.color.b);
                params.color.a = color.value()[4].get_or(params.color.a);
            }

            // Simple parameters
            params.frame = t["frame"].get_or(params.frame);
            params.scale = t["scale"].get_or(params.scale);
            params.center = t["center"].get_or(params.center);
            params.x = t["x"].get_or(params.x);
            params.y = t["y"].get_or(params.y);
            params.x2 = t["x2"].get_or(params.x2);
            params.y2 = t["y2"].get_or(params.y2);
            params.rotation = t["rotation"].get_or(params.rotation);
            params.fade = t["fade"].get_or(params.fade);
            params.index = t["index"].get_or(params.index);
            params.controllable = t["controllable"].get_or(params.controllable);

            // mirror (string)
            sol::optional<std::string> mirror = t["mirror"];
            if (mirror) {
                params.mirror = mirror.value();
            }

            // origin (as table {x, y})
            sol::optional<sol::table> origin = t["origin"];
            if (origin) {
                params.origin.x = origin.value()[1].get_or(params.origin.x);
                params.origin.y = origin.value()[2].get_or(params.origin.y);
            }

            // src (as table {x, y, width, height})
            sol::optional<sol::table> src = t["src"];
            if (src) {
                ray::Rectangle rect;
                rect.x = src.value()["x"].get_or(0.0f);
                rect.y = src.value()["y"].get_or(0.0f);
                rect.width = src.value()["width"].get_or(0.0f);
                rect.height = src.value()["height"].get_or(0.0f);
                params.src = rect;
            }
        }

        script_manager.tex.draw_texture(subset, texture_name, params);
    });

    lua["tex"] = tex;
}

ScriptManager script_manager;
