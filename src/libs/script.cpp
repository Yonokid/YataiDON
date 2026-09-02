#include "script.h"

#include <string_view>
#include <cstring>
#include "global_data.h"
#include "text.h"
#include "audio.h"
#include "input.h"
#include "filesystem.h"
#include "../objects/song_select/file_navigator/box_lua_bindings.h"
#include "../objects/enums.h"
#include <spdlog/spdlog.h>

static SessionData& current_session() {
    int idx = (int)global_data.player_num;
    if (idx < 0 || idx >= (int)global_data.session_data.size()) idx = 0;
    return global_data.session_data[idx];
}

static DrawTextureParams parse_draw_params_legacy(sol::optional<sol::table> params_table,
                                                  bool allow_blend) {
    DrawTextureParams params;
    if (!params_table) return params;
    sol::table t = params_table.value();

    sol::optional<sol::table> color = t["color"];
    if (color) {
        params.color.r = color.value()[1].get_or(params.color.r);
        params.color.g = color.value()[2].get_or(params.color.g);
        params.color.b = color.value()[3].get_or(params.color.b);
        params.color.a = color.value()[4].get_or(params.color.a);
    }

    params.frame    = t["frame"].get_or(params.frame);
    params.scale    = t["scale"].get_or(params.scale);
    params.center   = t["center"].get_or(params.center);
    params.x        = t["x"].get_or(params.x);
    params.y        = t["y"].get_or(params.y);
    params.x2       = t["x2"].get_or(params.x2);
    params.y2       = t["y2"].get_or(params.y2);
    params.rotation = t["rotation"].get_or(params.rotation);
    params.fade     = t["fade"].get_or(params.fade);
    params.index    = t["index"].get_or(params.index);

    sol::optional<std::string> mirror = t["mirror"];
    if (mirror) params.mirror = mirror_from_string(mirror.value());

    if (allow_blend) {
        sol::optional<std::string> blend = t["blend"];
        if (blend) params.blend = blend_from_string(blend.value());
    }

    sol::optional<sol::table> origin = t["origin"];
    if (origin) {
        params.origin.x = origin.value()[1].get_or(params.origin.x);
        params.origin.y = origin.value()[2].get_or(params.origin.y);
    }

    sol::optional<sol::table> src = t["src"];
    if (src) {
        ray::Rectangle rect;
        rect.x      = src.value()["x"].get_or(0.0f);
        rect.y      = src.value()["y"].get_or(0.0f);
        rect.width  = src.value()["width"].get_or(0.0f);
        rect.height = src.value()["height"].get_or(0.0f);
        params.src  = rect;
    }

    return params;
}

static DrawTextureParams parse_draw_params(sol::optional<sol::table> params_table,
                                            bool allow_blend = true) {
    return parse_draw_params_legacy(params_table, allow_blend);
}

static bool same_params(const DrawTextureParams& a, const DrawTextureParams& b) {
    if (a.color.r != b.color.r || a.color.g != b.color.g ||
        a.color.b != b.color.b || a.color.a != b.color.a) return false;
    if (a.frame != b.frame || a.scale != b.scale || a.center != b.center) return false;
    if (a.mirror != b.mirror) return false;
    if (a.x != b.x || a.y != b.y || a.x2 != b.x2 || a.y2 != b.y2) return false;
    if (a.origin.x != b.origin.x || a.origin.y != b.origin.y) return false;
    if (a.rotation != b.rotation || a.fade != b.fade || a.index != b.index) return false;
    if (a.src.has_value() != b.src.has_value()) return false;
    if (a.src && (a.src->x != b.src->x || a.src->y != b.src->y ||
                  a.src->width != b.src->width || a.src->height != b.src->height))
        return false;
    if (a.blend != b.blend) return false;
    return true;
}

// Index every script under one Scripts folder. Names already present are
// kept: the skin's own scripts are indexed first, and a parent skin only
// fills the gaps, the same way its graphics do.
void ScriptManager::index_scripts(const fs::path& script_path) {
    std::error_code ec;
    for (const auto& script : fs::directory_iterator(script_path, ec)) {
        fs::path p = script.path();
        if (fs::is_directory(p)) {
            fs::path lua_file = p / (p.stem().string() + ".lua");
            if (fs::exists(lua_file) && !scripts.count(p.stem().string())) {
                scripts[p.stem().string()] = lua_file.string();
            }
            for (const auto& sub : fs::directory_iterator(p)) {
                fs::path sub_p = sub.path();
                if (!fs::is_directory(sub_p) && sub_p.extension() == ".lua" && sub_p.stem() != p.stem() &&
                    !scripts.count(sub_p.stem().string())) {
                    scripts[sub_p.stem().string()] = sub_p.string();
                }
            }
        } else if (p.extension() == ".lua" && !scripts.count(p.stem().string())) {
            scripts[p.stem().string()] = p.string();
        }
    }
}

void ScriptManager::init(fs::path script_path) {
    lua = std::make_unique<sol::state>();
    lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::string,
                        sol::lib::math, sol::lib::table);

    // A partial skin scripts only some screens and leans on its parent for
    // the rest, exactly like its graphics.
    fs::path parent_scripts;
    if (skin_has_parent())
        parent_scripts = parent_skin_root() / "Scripts";

    std::string skin_scripts_dir = script_path.string();
    std::string package_path = skin_scripts_dir + "/?.lua;" +
                               skin_scripts_dir + "/?/init.lua";
    if (!parent_scripts.empty()) {
        package_path += ";" + parent_scripts.string() + "/?.lua;" +
                        parent_scripts.string() + "/?/init.lua";
    }
    (*lua)["package"]["path"] = package_path;

    index_scripts(script_path);
    if (!parent_scripts.empty()) index_scripts(parent_scripts);

    spdlog::debug("Loaded scripts:");
    for (const auto& [name, path] : scripts) {
        spdlog::debug("  {} -> {}", name, path);
    }
    spdlog::debug("Total scripts: {}", scripts.size());

    tex.init(script_path.parent_path() / "Graphics");

    register_lua_bindings();
}

bool ScriptManager::has_lua_script(const std::string& script_name) const {
    return scripts.count(script_name) != 0;
}

std::string ScriptManager::get_lua_script_path(const std::string& script_name) {
    if (scripts.find(script_name) == scripts.end()) {
        throw std::runtime_error("Unable to find lua script: " + script_name);
    }
    return scripts[script_name];
}

void ScriptManager::shutdown() {
    script_manager.tex.unload_textures();
    lua.reset();
}

void ScriptManager::register_lua_bindings() {
    sol::state& lua = *this->lua;
    lua.new_usertype<BaseAnimation>("BaseAnimation",
        "update", [](BaseAnimation& self, double t) { self.update(t); return self.attribute; },
        "restart", &BaseAnimation::restart,
        "start", &BaseAnimation::start,
        "pause", &BaseAnimation::pause,
        "unpause", &BaseAnimation::unpause,
        "reset", &BaseAnimation::reset,
        "attribute", &BaseAnimation::attribute,
        "duration", &BaseAnimation::duration,
        "is_finished", &BaseAnimation::is_finished,
        "is_started", &BaseAnimation::is_started,
        "isFinished", &BaseAnimation::isFinished,
        "isStarted", &BaseAnimation::isStarted
    );

    // Fade animation bindings
    lua.new_usertype<FadeAnimation>("FadeAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", [](FadeAnimation& self, double t) { self.update(t); return self.attribute; },
        "restart", &FadeAnimation::restart
    );

    // Move animation bindings
    lua.new_usertype<MoveAnimation>("MoveAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", [](MoveAnimation& self, double t) { self.update(t); return self.attribute; },
        "restart", &MoveAnimation::restart
    );

    // Texture change animation bindings
    lua.new_usertype<TextureChangeAnimation>("TextureChangeAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", [](TextureChangeAnimation& self, double t) { self.update(t); return self.attribute; },
        "reset", &TextureChangeAnimation::reset
    );

    // Text stretch animation bindings
    lua.new_usertype<TextStretchAnimation>("TextStretchAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", [](TextStretchAnimation& self, double t) { self.update(t); return self.attribute; }
    );

    // Texture resize animation bindings
    lua.new_usertype<TextureResizeAnimation>("TextureResizeAnimation",
        sol::base_classes, sol::bases<BaseAnimation>(),
        "update", [](TextureResizeAnimation& self, double t) { self.update(t); return self.attribute; },
        "restart", &TextureResizeAnimation::restart
    );

    // Animation creation helper functions
    sol::table anim = lua.create_table();

    anim.set_function("fade", [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<FadeAnimation> {
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

        return std::make_unique<FadeAnimation>(duration, initial_opacity, loop, lock_input, final_opacity, delay, ease_in, ease_out, reverse_delay);
    });

    anim.set_function("move", [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<MoveAnimation> {
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

        return std::make_unique<MoveAnimation>(duration, total_distance, loop, lock_input, start_position, delay, reverse_delay, ease_in, ease_out);
    });

    anim.set_function("texture_change", [](double duration, sol::table textures_table, sol::optional<sol::table> params) -> std::unique_ptr<TextureChangeAnimation> {
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

        return std::make_unique<TextureChangeAnimation>(duration, keyframes, loop, lock_input, delay);
    });

    anim.set_function("text_stretch", [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<TextStretchAnimation> {
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;

        if (params) {
            sol::table t = params.value();
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);
        }

        return std::make_unique<TextStretchAnimation>(duration, delay, loop, lock_input);
    });

    anim.set_function("texture_resize", [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<TextureResizeAnimation> {
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

        return std::make_unique<TextureResizeAnimation>(duration, initial_size, loop, lock_input, final_size, delay, reverse_delay, ease_in, ease_out);
    });

    lua["anim"] = anim;

    sol::table tex = lua.create_table();

    tex.set_function("load_animations", [](const std::string& screen_name) {
        script_manager.tex.load_animations(screen_name);
    });

    tex.set_function("get_animation", [](int anim_id, sol::object second_arg) -> BaseAnimation* {
        if (second_arg.get_type() == sol::type::string)
            return script_manager.tex.get_animation(anim_id, second_arg.as<std::string>());
        return script_manager.tex.get_animation(anim_id, false);
    });

    tex.set_function("load_folder", [](const std::string& screen_name, const std::string& subset) {
        script_manager.tex.load_folder(screen_name, subset);
    });

    tex.set_function("unload_folder", [](const std::string& screen_name, const std::string& subset) {
        script_manager.tex.unload_folder(screen_name, subset);
    });

    tex.set_function("current_screen", []() -> std::string {
        return global_data.current_screen;
    });

    tex.set_function("in_transition", []() -> bool {
        return global_data.in_transition;
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
        auto config_it = script_manager.tex.skin_config_by_name.find(config_key);
        if (config_it == script_manager.tex.skin_config_by_name.end()) {
            return sol::nullopt;
        }

        const auto& skin_info = config_it->second;

        sol::table info = script_manager.lua->create_table();
        info["x"] = skin_info.x;
        info["y"] = skin_info.y;
        info["font_size"] = skin_info.font_size;
        info["width"] = skin_info.width;
        info["height"] = skin_info.height;

        return info;
    });

    tex.set_function("get_texture_keys", [](const std::string& subset) -> sol::optional<sol::table> {
        std::string prefix = subset + "/";
        sol::table keys = script_manager.lua->create_table();
        int index = 1;
        for (const auto& [path, id] : tex_id_map) {
            if (path.size() > prefix.size() && path.substr(0, prefix.size()) == prefix) {
                if (script_manager.tex.textures.find(id) != script_manager.tex.textures.end()) {
                    keys[index] = path.substr(prefix.size());
                    ++index;
                }
            }
        }
        if (index == 1) return sol::nullopt;
        return keys;
    });

    tex.set_function("get_texture_info", [](const std::string& subset, const std::string& texture_name) -> sol::optional<sol::table> {
        auto it = tex_id_map.find(subset + "/" + texture_name);
        if (it == tex_id_map.end()) return sol::nullopt;

        auto tex_it = script_manager.tex.textures.find(it->second);
        if (tex_it == script_manager.tex.textures.end()) return sol::nullopt;

        const auto& tex_obj = tex_it->second;

        sol::table info = script_manager.lua->create_table();
        info["name"] = tex_obj->name;
        info["x"] = sol::as_table(tex_obj->x);
        info["y"] = sol::as_table(tex_obj->y);
        info["x2"] = sol::as_table(tex_obj->x2);
        info["y2"] = sol::as_table(tex_obj->y2);
        info["width"] = tex_obj->width;
        info["height"] = tex_obj->height;

        int frame_count = 1;
        if (auto framed = dynamic_cast<FramedTexture*>(tex_obj.get())) {
            frame_count = static_cast<int>(framed->textures.size());
        }
        if (tex_obj->crop_data.has_value()) {
            frame_count = std::max(frame_count, static_cast<int>(tex_obj->crop_data->size()));
        }
        info["frame_count"] = frame_count;

        return info;
    });

    tex.set_function("get_id", [](const std::string& subset, const std::string& texture_name) -> sol::optional<uint32_t> {
        auto it = tex_id_map.find(subset + "/" + texture_name);
        if (it != tex_id_map.end()) return it->second;
        it = tex_id_map.find(subset + "/" + texture_name + "_" + global_data.config->general.language);
        if (it != tex_id_map.end()) return it->second;
        return std::nullopt;
    });

    tex.set_function("draw_texture", [](uint32_t id, sol::optional<sol::table> params_table) {
        script_manager.tex.draw_texture(id, parse_draw_params(params_table));
    });

    tex.set_function("load_texture", [](const std::string& path) -> sol::optional<uint32_t> {
        // path format: "screen_name/subset/texture_name", e.g. "global/indicator/drum_face"
        auto first_slash = path.find('/');
        if (first_slash == std::string::npos) return sol::nullopt;
        auto second_slash = path.find('/', first_slash + 1);
        if (second_slash == std::string::npos) return sol::nullopt;

        std::string screen_name  = path.substr(0, first_slash);
        std::string subset       = path.substr(first_slash + 1, second_slash - first_slash - 1);
        std::string texture_name = path.substr(second_slash + 1);

        script_manager.tex.load_folder(screen_name, subset);

        auto it = tex_id_map.find(subset + "/" + texture_name + "_" + global_data.config->general.language);
        if (it != tex_id_map.end()) return static_cast<uint32_t>(it->second);
        it = tex_id_map.find(subset + "/" + texture_name);
        if (it != tex_id_map.end()) return static_cast<uint32_t>(it->second);
        return sol::nullopt;
    });

    tex.set_function("language", []() { return global_data.config->general.language; });

    tex.set_function("skip_enabled", []() { return global_data.live_skip_count >= 0; });

    lua["tex"] = tex;

    lua.new_usertype<OutlinedText>("OutlinedText",
        "width",          &OutlinedText::width,
        "height",         &OutlinedText::height,
        "is_ready",       &OutlinedText::is_ready,
        "upload_pending", &OutlinedText::upload_pending,
        "finish",         &OutlinedText::finish,
        "draw",           [](OutlinedText& self, sol::optional<sol::table> params_table) {
            DrawTextureParams params = parse_draw_params(params_table, false);
            self.draw(params);
        }
    );

    sol::table text = lua.create_table();
    text.set_function("create_text", [](const std::string& skin_config_key, std::array<int, 4> color,
        std::array<int, 4> outline_color, bool is_vertical, float outline_thickness, float spacing) -> std::unique_ptr<OutlinedText> {
            auto config_it = script_manager.tex.skin_config_by_name.find(skin_config_key);
            if (config_it == script_manager.tex.skin_config_by_name.end()) {
                spdlog::error("Skin config key not found: {}", skin_config_key);
                return nullptr;
            }
            int font_size = config_it->second.font_size;
            std::string text = config_it->second.text[global_data.config->general.language];
            ray::Color color_val;
            color_val.r = color[0];
            color_val.g = color[1];
            color_val.b = color[2];
            color_val.a = color[3];
            ray::Color outline_color_val;
            outline_color_val.r = outline_color[0];
            outline_color_val.g = outline_color[1];
            outline_color_val.b = outline_color[2];
            outline_color_val.a = outline_color[3];
            std::unique_ptr<OutlinedText> ptr = std::make_unique<OutlinedText>(text, font_size, color_val, outline_color_val, is_vertical, outline_thickness, spacing);
            ptr->x_offset = config_it->second.x;
            ptr->y_offset = config_it->second.y;
            return ptr;
    });

    text.set_function("create_raw_text", [](const std::string& content, int font_size,
        std::array<int, 4> color, std::array<int, 4> outline_color,
        bool is_vertical, sol::optional<float> thickness, sol::optional<float> spacing)
        -> std::unique_ptr<OutlinedText> {
            ray::Color c  = { (uint8_t)color[0],         (uint8_t)color[1],         (uint8_t)color[2],         (uint8_t)color[3] };
            ray::Color oc = { (uint8_t)outline_color[0], (uint8_t)outline_color[1], (uint8_t)outline_color[2], (uint8_t)outline_color[3] };
            return std::make_unique<OutlinedText>(content, font_size, c, oc, is_vertical,
                thickness.value_or(5.0f), spacing.value_or(2.0f));
    });

    lua["text"] = text;

    tex.set_function("get_current_ms", []() -> double {
        return get_current_ms();
    });

    tex.set_function("player_num", []() -> int {
        return (int)global_data.player_num;
    });

    tex.set_function("display_bpm", []() -> bool {
        return global_data.config->general.display_bpm;
    });

    tex.set_function("song_title", []() -> std::string {
        return current_session().song_title;
    });

    tex.set_function("song_subtitle", []() -> std::string {
        return current_session().song_subtitle;
    });

    tex.set_function("song_genre", []() -> int {
        return current_session().genre_index;
    });

    tex.set_function("genre_frame", [](int genre_index) -> int {
        return genre_to_ref_frame((GenreIndex)genre_index);
    });

    tex.set_function("song_number", []() -> int {
        return global_data.songs_played + 1;
    });

    tex.set_function("songs_played", []() -> int {
        return global_data.songs_played;
    });

    tex.set_function("song_limit", []() -> int {
        return global_data.config->general.song_limit;
    });

    tex.set_function("last_input_ms", []() -> double {
        return get_last_input_ms();
    });

    tex.set_function("log", [](const std::string& msg) {
        spdlog::info("[lua] {}", msg);
    });

    sol::table audio_tbl = lua.create_table();

    audio_tbl.set_function("play_sound", [](const std::string& name, sol::optional<std::string> preset_str,
                                            sol::optional<bool> loop) {
        VolumePreset preset = VolumePreset::NONE;
        if (preset_str) {
            const std::string& p = preset_str.value();
            if      (p == "sound")    preset = VolumePreset::SOUND;
            else if (p == "music")    preset = VolumePreset::MUSIC;
            else if (p == "voice")    preset = VolumePreset::VOICE;
            else if (p == "hitsound") preset = VolumePreset::HITSOUND;
        }
        audio.set_sound_loop(name, loop.value_or(false));
        audio.play_sound(name, preset);
    });

    audio_tbl.set_function("stop_sound", [](const std::string& name) {
        audio.set_sound_loop(name, false);
        audio.stop_sound(name);
    });

    audio_tbl.set_function("is_sound_playing", [](const std::string& name) -> bool {
        return audio.is_sound_playing(name);
    });

    lua["audio"] = audio_tbl;

    register_song_select_lua_bindings(lua);
}

ScriptManager script_manager;
