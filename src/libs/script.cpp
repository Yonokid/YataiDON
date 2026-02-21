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
        "restart", &BaseAnimation::restart,
        "start", &BaseAnimation::start,
        "pause", &BaseAnimation::pause,
        "unpause", &BaseAnimation::unpause,
        "reset", &BaseAnimation::reset,
        "attribute", &BaseAnimation::attribute,
        "is_finished", &BaseAnimation::is_finished,
        "is_started", &BaseAnimation::is_started,
        "isFinished", &BaseAnimation::isFinished,
        "isStarted", &BaseAnimation::isStarted
    );

    // Fade animation bindings
    lua.new_usertype<FadeAnimation>("FadeAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", &FadeAnimation::update,
        "restart", &FadeAnimation::restart
    );

    // Move animation bindings
    lua.new_usertype<MoveAnimation>("MoveAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", &MoveAnimation::update,
        "restart", &MoveAnimation::restart
    );

    // Texture change animation bindings
    lua.new_usertype<TextureChangeAnimation>("TextureChangeAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", &TextureChangeAnimation::update,
        "reset", &TextureChangeAnimation::reset
    );

    // Text stretch animation bindings
    lua.new_usertype<TextStretchAnimation>("TextStretchAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", &TextStretchAnimation::update
    );

    // Texture resize animation bindings
    lua.new_usertype<TextureResizeAnimation>("TextureResizeAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", &TextureResizeAnimation::update,
        "restart", &TextureResizeAnimation::restart
    );

    // Animation creation helper functions
    sol::table anim = lua.create_table();

    anim.set_function("fade", [](double duration, sol::optional<sol::table> params) -> FadeAnimation* {
        double initial_opacity = 1.0;
        double final_opacity = 0.0;
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;
        std::optional<std::string> ease_in = std::nullopt;
        std::optional<std::string> ease_out = std::nullopt;
        std::optional<double> reverse_delay = std::nullopt;

        if (params) {
            sol::table t = params.value();
            initial_opacity = t["initial_opacity"].get_or(initial_opacity);
            final_opacity = t["final_opacity"].get_or(final_opacity);
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);

            sol::optional<std::string> ease_in_opt = t["ease_in"];
            if (ease_in_opt) ease_in = ease_in_opt.value();

            sol::optional<std::string> ease_out_opt = t["ease_out"];
            if (ease_out_opt) ease_out = ease_out_opt.value();

            sol::optional<double> reverse_delay_opt = t["reverse_delay"];
            if (reverse_delay_opt) reverse_delay = reverse_delay_opt.value();
        }

        return new FadeAnimation(duration, initial_opacity, loop, lock_input, final_opacity, delay, ease_in, ease_out, reverse_delay);
    });

    anim.set_function("move", [](double duration, sol::optional<sol::table> params) -> MoveAnimation* {
        int total_distance = 0;
        int start_position = 0;
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;
        std::optional<double> reverse_delay = std::nullopt;
        std::optional<std::string> ease_in = std::nullopt;
        std::optional<std::string> ease_out = std::nullopt;

        if (params) {
            sol::table t = params.value();
            total_distance = t["total_distance"].get_or(total_distance);
            start_position = t["start_position"].get_or(start_position);
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);

            sol::optional<double> reverse_delay_opt = t["reverse_delay"];
            if (reverse_delay_opt) reverse_delay = reverse_delay_opt.value();

            sol::optional<std::string> ease_in_opt = t["ease_in"];
            if (ease_in_opt) ease_in = ease_in_opt.value();

            sol::optional<std::string> ease_out_opt = t["ease_out"];
            if (ease_out_opt) ease_out = ease_out_opt.value();
        }

        return new MoveAnimation(duration, total_distance, loop, lock_input, start_position, delay, reverse_delay, ease_in, ease_out);
    });

    anim.set_function("texture_change", [](double duration, sol::table textures_table, sol::optional<sol::table> params) -> TextureChangeAnimation* {
        std::vector<std::tuple<double, double, int>> keyframes;

        for (size_t i = 1; i <= textures_table.size(); ++i) {
            sol::table tex_entry = textures_table[i];
            double start = tex_entry[1].get<double>();
            double end = tex_entry[2].get<double>();
            int index = tex_entry[3].get<int>();
            keyframes.emplace_back(start, end, index);
        }

        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;

        if (params) {
            sol::table t = params.value();
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);
        }

        return new TextureChangeAnimation(duration, keyframes, loop, lock_input, delay);
    });

    anim.set_function("text_stretch", [](double duration, sol::optional<sol::table> params) -> TextStretchAnimation* {
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;

        if (params) {
            sol::table t = params.value();
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);
        }

        return new TextStretchAnimation(duration, delay, loop, lock_input);
    });

    anim.set_function("texture_resize", [](double duration, sol::optional<sol::table> params) -> TextureResizeAnimation* {
        double initial_size = 1.0;
        double final_size = 0.0;
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;
        std::optional<double> reverse_delay = std::nullopt;
        std::optional<std::string> ease_in = std::nullopt;
        std::optional<std::string> ease_out = std::nullopt;

        if (params) {
            sol::table t = params.value();
            initial_size = t["initial_size"].get_or(initial_size);
            final_size = t["final_size"].get_or(final_size);
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);

            sol::optional<double> reverse_delay_opt = t["reverse_delay"];
            if (reverse_delay_opt) reverse_delay = reverse_delay_opt.value();

            sol::optional<std::string> ease_in_opt = t["ease_in"];
            if (ease_in_opt) ease_in = ease_in_opt.value();

            sol::optional<std::string> ease_out_opt = t["ease_out"];
            if (ease_out_opt) ease_out = ease_out_opt.value();
        }

        return new TextureResizeAnimation(duration, initial_size, loop, lock_input, final_size, delay, reverse_delay, ease_in, ease_out);
    });

    lua["anim"] = anim;

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

    tex.set_function("get_screen_width", []() -> float {
        return script_manager.tex.screen_width;
    });

    tex.set_function("get_screen_height", []() -> float {
        return script_manager.tex.screen_height;
    });

    tex.set_function("get_screen_scale", []() -> float {
        return script_manager.tex.screen_scale;
    });

    tex.set_function("get_skin_config", [](const std::string& config_key) -> sol::optional<sol::table> {
        auto config_it = script_manager.tex.skin_config.find(config_key);
        if (config_it == script_manager.tex.skin_config.end()) {
            return sol::nullopt;
        }

        const auto& skin_info = config_it->second;

        sol::table info = script_manager.lua.create_table();
        info["x"] = skin_info.x;
        info["y"] = skin_info.y;
        info["font_size"] = skin_info.font_size;
        info["width"] = skin_info.width;
        info["height"] = skin_info.height;

        return info;
    });

    tex.set_function("get_texture_keys", [](const std::string& subset) -> sol::optional<sol::table> {
        auto subset_it = script_manager.tex.textures.find(subset);
        if (subset_it == script_manager.tex.textures.end()) {
            return sol::nullopt;
        }

        sol::table keys = script_manager.lua.create_table();
        int index = 1;
        for (const auto& [key, _] : subset_it->second) {
            keys[index] = key;
            ++index;
        }

        return keys;
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
        
        // Try to get frame count from the texture object
        int frame_count = 1;
        if (auto framed = dynamic_cast<FramedTexture*>(tex_obj.get())) {
            frame_count = framed->textures.size();
        }
        info["frame_count"] = frame_count;

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
