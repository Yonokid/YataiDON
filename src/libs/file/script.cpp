#include "script.h"
#ifdef TRACY_ENABLE
#include <tracy/TracyLua.hpp>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <spdlog/spdlog.h>
#include <string_view>

#include "../../objects/enums.h"
#include "../../objects/song_select/file_navigator/box_lua_bindings.h"
#include "../audio/audio.h"
#include "../gfx/text.h"
#include "../global_data.h"
#include "../input/input.h"
#include "../screen.h"
#include "../video/webcam.h"
#include "filesystem.h"

namespace {
    constexpr int LUA_SITE_MAX_LEVELS = 8;

    struct LuaStackGuard {
        lua_State* state;
        int top;
        explicit LuaStackGuard(lua_State* state) : state(state), top(lua_gettop(state)) {}
        ~LuaStackGuard() { lua_settop(state, top); }
    };
} // namespace

void log_lua_site(DrawLogEntry& entry, lua_State* state) {
    if (!state) {
        return;
    }
    LuaStackGuard guard(state);
    lua_Debug frame;
    for (int level = 0; level < LUA_SITE_MAX_LEVELS; level++) {
        if (!lua_getstack(state, level, &frame) || !lua_getinfo(state, "Sln", &frame)) {
            return;
        }
        if (frame.what && frame.what[0] == 'C') {
            continue;
        }
        entry.from_lua = true;
        entry.lua_line = frame.currentline;
        entry.lua_defined_line = frame.linedefined;
        entry.lua_function = frame.name ? frame.name : "";
        if (frame.source && frame.source[0] == '@') {
            entry.lua_source = frame.source + 1;
        }
        return;
    }
}

static std::optional<EaseType> parse_ease_type(const sol::optional<std::string>& ease_str) {
    if (!ease_str) {
        return std::nullopt;
    }
    if (ease_str == "quadratic") {
        return EaseType::Quadratic;
    }
    if (ease_str == "cubic") {
        return EaseType::Cubic;
    }
    if (ease_str == "exponential") {
        return EaseType::Exponential;
    }
    spdlog::error("Unknown ease type: {}", ease_str.value());
    return std::nullopt;
}

static SessionData& current_session() {
    static SessionData fallback{};
    if (global_data.session_data.empty()) {
        return fallback;
    }
    int idx = static_cast<int>(global_data.player_num);
    if (idx < 0 || idx >= std::ssize(global_data.session_data)) {
        idx = 0;
    }
    return global_data.session_data[idx];
}

static DrawTextureParams parse_draw_params_legacy(const sol::optional<sol::table>& params_table) {
    DrawTextureParams params;
    if (!params_table) {
        return params;
    }
    sol::table t = params_table.value();

    sol::optional<sol::table> color = t["color"];
    if (color) {
        params.color.r = color.value()[1].get_or(params.color.r);
        params.color.g = color.value()[2].get_or(params.color.g);
        params.color.b = color.value()[3].get_or(params.color.b);
        params.color.a = color.value()[4].get_or(params.color.a);
    }

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

    sol::optional<std::string> mirror = t["mirror"];
    if (mirror) {
        params.mirror = mirror_from_string(mirror.value());
    }

    sol::optional<sol::table> origin = t["origin"];
    if (origin) {
        params.origin.x = origin.value()[1].get_or(params.origin.x);
        params.origin.y = origin.value()[2].get_or(params.origin.y);
    }

    sol::optional<sol::table> src = t["src"];
    if (src) {
        Rect rect;
        rect.x = src.value()["x"].get_or(0.0f);
        rect.y = src.value()["y"].get_or(0.0f);
        rect.width = src.value()["width"].get_or(0.0f);
        rect.height = src.value()["height"].get_or(0.0f);
        params.src = rect;
    }

    return params;
}

// One value the way sol's get_or reads it in release builds: anything but a number keeps the
// default, integers convert directly, other numbers round to the nearest integer for integral T.
template<typename T>
static T lua_number_or(lua_State* L, int idx, T def) {
    if (lua_type(L, idx) != LUA_TNUMBER) {
        return def;
    }
    if constexpr (std::is_integral_v<T>) {
        if (lua_isinteger(L, idx)) {
            return static_cast<T>(lua_tointeger(L, idx));
        }
        return static_cast<T>(llround(lua_tonumber(L, idx)));
    } else {
        return static_cast<T>(lua_tonumber(L, idx));
    }
}

// t[i] / t.key of the table or userdata at idx, metamethods included (like sol's proxies)
template<typename T>
static T lua_index_or(lua_State* L, int idx, lua_Integer i, T def) {
    lua_geti(L, idx, i);
    T v = lua_number_or(L, -1, def);
    lua_pop(L, 1);
    return v;
}
template<typename T>
static T lua_field_or(lua_State* L, int idx, const char* key, T def) {
    lua_getfield(L, idx, key);
    T v = lua_number_or(L, -1, def);
    lua_pop(L, 1);
    return v;
}

static bool lua_is_table_like(lua_State* L, int idx) {
    const int t = lua_type(L, idx);
    return t == LUA_TTABLE || t == LUA_TUSERDATA;
}

// Same result as parse_draw_params_legacy, but walks only the keys the table has (lua_next)
// instead of looking up all ~17 names through sol proxies: draw_texture is called a few hundred
// times per frame by Lua skins and the lookups were most of its cost. A table with a metatable
// (fields could come from __index) or a userdata goes through the legacy path.
static DrawTextureParams parse_draw_params(const sol::optional<sol::table>& params_table) {
    if (!params_table) {
        return DrawTextureParams{};
    }
    lua_State* L = params_table->lua_state();
    params_table->push();
    const int t = lua_gettop(L);
    if (lua_type(L, t) != LUA_TTABLE || lua_getmetatable(L, t)) {
        lua_settop(L, t - 1);
        return parse_draw_params_legacy(params_table);
    }

    DrawTextureParams params;
    lua_pushnil(L);
    while (lua_next(L, t) != 0) {
        // key at -2, value at -1
        if (lua_type(L, -2) == LUA_TSTRING) {
            size_t len = 0;
            const char* k = lua_tolstring(L, -2, &len);
            const std::string_view key(k, len);
            const int v = lua_gettop(L);
            if (key == "x") {
                params.x = lua_number_or(L, v, params.x);
            } else if (key == "y") {
                params.y = lua_number_or(L, v, params.y);
            } else if (key == "x2") {
                params.x2 = lua_number_or(L, v, params.x2);
            } else if (key == "y2") {
                params.y2 = lua_number_or(L, v, params.y2);
            } else if (key == "fade") {
                params.fade = lua_number_or(L, v, params.fade);
            } else if (key == "frame") {
                params.frame = lua_number_or(L, v, params.frame);
            } else if (key == "scale") {
                params.scale = lua_number_or(L, v, params.scale);
            } else if (key == "rotation") {
                params.rotation = lua_number_or(L, v, params.rotation);
            } else if (key == "index") {
                params.index = lua_number_or(L, v, params.index);
            } else if (key == "center") {
                if (lua_isboolean(L, v)) {
                    params.center = lua_toboolean(L, v) != 0;
                }
            } else if (key == "color") {
                if (lua_is_table_like(L, v)) {
                    params.color.r = lua_index_or(L, v, 1, params.color.r);
                    params.color.g = lua_index_or(L, v, 2, params.color.g);
                    params.color.b = lua_index_or(L, v, 3, params.color.b);
                    params.color.a = lua_index_or(L, v, 4, params.color.a);
                }
            } else if (key == "src") {
                if (lua_is_table_like(L, v)) {
                    Rect rect;
                    rect.x = lua_field_or(L, v, "x", 0.0f);
                    rect.y = lua_field_or(L, v, "y", 0.0f);
                    rect.width = lua_field_or(L, v, "width", 0.0f);
                    rect.height = lua_field_or(L, v, "height", 0.0f);
                    params.src = rect;
                }
            } else if (key == "origin") {
                if (lua_is_table_like(L, v)) {
                    params.origin.x = lua_index_or(L, v, 1, params.origin.x);
                    params.origin.y = lua_index_or(L, v, 2, params.origin.y);
                }
            } else if (key == "mirror") {
                if (lua_type(L, v) == LUA_TSTRING) {
                    params.mirror = mirror_from_string(lua_tostring(L, v));
                }
            }
        }
        lua_pop(L, 1);
    }
    lua_settop(L, t - 1);
    return params;
}

// Index every script under one Scripts folder. Names already present are
// kept: the skin's own scripts are indexed first, and a parent skin only
// fills the gaps, the same way its graphics do.
void ScriptManager::index_scripts(const fs::path& script_path) {
    std::error_code ec;
    fs::directory_iterator dir(script_path, ec);
    if (ec) {
        spdlog::warn("Unable to index scripts in {}: {}", script_path.string(), ec.message());
        return;
    }
    for (const auto& script : dir) {
        const fs::path& p = script.path();
        if (fs::is_directory(p)) {
            fs::path lua_file = p / (p.stem().string() + ".lua");
            if (fs::exists(lua_file) && !scripts.contains(p.stem().string())) {
                scripts[p.stem().string()] = lua_file.string();
            }
            std::error_code sub_ec;
            for (const auto& sub : fs::directory_iterator(p, sub_ec)) {
                const fs::path& sub_p = sub.path();
                if (!fs::is_directory(sub_p) && sub_p.extension() == ".lua" && sub_p.stem() != p.stem() &&
                    !scripts.contains(sub_p.stem().string())) {
                    scripts[sub_p.stem().string()] = sub_p.string();
                }
            }
            if (sub_ec) {
                spdlog::warn("Unable to index scripts in {}: {}", p.string(), sub_ec.message());
            }
        } else if (p.extension() == ".lua" && !scripts.contains(p.stem().string())) {
            scripts[p.stem().string()] = p.string();
        }
    }
}

void ScriptManager::init(const fs::path& script_path) {
    lua = std::make_unique<sol::state>();
    lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::math, sol::lib::table);
#ifdef TRACY_ENABLE
    tracy::LuaRegister(lua->lua_state()); // tracy.ZoneBeginN / ZoneEnd / ZoneText / Message for skins
#endif

    scripts.clear();
    executed_scripts.clear();

    // the parent's Scripts, and its parent's ..., nearest first
    std::vector<fs::path> ancestor_scripts;
    for (const fs::path& root : skin_ancestor_roots()) {
        ancestor_scripts.push_back(root / "Scripts");
    }

    std::string skin_scripts_dir = script_path.string();
    std::string package_path = skin_scripts_dir + "/?.lua;" + skin_scripts_dir + "/?/init.lua";
    for (const fs::path& dir : ancestor_scripts) {
        package_path += ";" + dir.string() + "/?.lua;" + dir.string() + "/?/init.lua";
    }
    std::string default_path = (*lua)["package"]["path"];
    (*lua)["package"]["path"] = package_path + ";" + default_path;

    std::vector<std::string> script_dirs{skin_scripts_dir};
    for (const fs::path& dir : ancestor_scripts) {
        script_dirs.push_back(dir.string());
    }
    sol::function make_require = lua->load(R"(
        local orig, caller_source, dirs = ...
        local searchpath, loaded = package.searchpath, package.loaded
        local function path_from(k)
            local p = {}
            for i = k, #dirs do p[#p + 1] = dirs[i] .. "/?.lua;" .. dirs[i] .. "/?/init.lua" end
            return table.concat(p, ";")
        end
        return function(name)
            local src = caller_source()
            for k = 2, #dirs do
                if src:sub(1, #dirs[k] + 2) == "@" .. dirs[k] .. "/" then
                    local file = searchpath(name, path_from(k))
                    if not file or file == searchpath(name, package.path) then break end
                    if loaded[file] == nil then
                        local m = assert(loadfile(file))(name, file)
                        loaded[file] = m == nil and true or m
                    end
                    return loaded[file], file
                end
            end
            return orig(name)
        end
    )",
                                           "=skin_require");
    auto caller_source = [](sol::this_state s) -> std::string {
        lua_Debug ar; // level 0 = this function, 1 = the require wrapper, 2 = its caller
        if (!lua_getstack(s, 2, &ar) || !lua_getinfo(s, "S", &ar) || !ar.source) {
            return "";
        }
        return ar.source;
    };
    (*lua)["require"] = make_require.call<sol::function>((*lua)["require"], sol::make_object(*lua, caller_source),
                                                         sol::as_table(script_dirs));

    index_scripts(script_path);
    for (const fs::path& dir : ancestor_scripts) {
        index_scripts(dir);
    }

    spdlog::debug("Loaded scripts:");
    for (const auto& [name, path] : scripts) {
        spdlog::debug("  {} -> {}", name, path);
    }
    spdlog::debug("Total scripts: {}", scripts.size());

    // script_manager.tex aliases the global ::tex, already initialized by
    // load_skin() just before ScriptManager::init is called.

    register_lua_bindings();
}

bool ScriptManager::has_lua_script(const std::string& script_name) const {
    return scripts.contains(script_name);
}

std::string ScriptManager::get_lua_script_path(const std::string& script_name) {
    if (!scripts.contains(script_name)) {
        throw std::runtime_error("Unable to find lua script: " + script_name);
    }
    return scripts[script_name];
}

void ScriptManager::shutdown() {
    // Don't unload textures here: tex aliases the global ::tex, whose lifetime
    // is managed by load_skin()/unload_skin() and main().
    scripts.clear();
    executed_scripts.clear();
    lua.reset();
}

// tex.get_texture: "screen/subset/name" -> the texture, loading the subset folder on a miss
// TODO: Thanks to @Ape, this resolves the crash on latest commit currently, but it's a complete hack
//       that needs a rewrite.  -QBaraki
static sol::optional<std::shared_ptr<TextureObject>> resolve_lua_texture(const std::string& path) {
    auto first_slash = path.find('/');
    auto last_slash = path.rfind('/');
    if (first_slash == std::string::npos) {
        return sol::nullopt;
    }

    std::string screen_name = last_slash == first_slash ? "game" : path.substr(0, first_slash);
    std::string subset = last_slash == first_slash ? path.substr(0, first_slash)
                                                   : path.substr(first_slash + 1, last_slash - first_slash - 1);
    std::string texture_name = path.substr(last_slash + 1);

    std::string subset_key = fs::path(subset).filename().string();
    std::string base = subset_key + "/" + texture_name;

    for (const auto& v : script_manager.tex.language_variants(base + "_" + global_data.config->general.language)) {
        if (script_manager.tex.has_texture(v)) {
            return script_manager.tex.get_texture_shared(v);
        }
    }
    if (script_manager.tex.has_texture(base)) {
        return script_manager.tex.get_texture_shared(base);
    }

    script_manager.tex.load_folder(screen_name, subset);

    for (const auto& v : script_manager.tex.language_variants(base + "_" + global_data.config->general.language)) {
        if (script_manager.tex.has_texture(v)) {
            return script_manager.tex.get_texture_shared(v);
        }
    }
    if (script_manager.tex.has_texture(base)) {
        return script_manager.tex.get_texture_shared(base);
    }
    return sol::nullopt;
}

// sol2 gives a usertype that binds member variables (or has base classes) a C __index, and
// that __index pushes a new C closure every time a method is looked up: each `anim:update(ms)`
// or `box:lua_kind()` from a skin allocated a GC object before the call. The wrapper below
// answers keys that resolved to a function from a per-metatable cache (a method is the same
// closure for every instance of the type: sol's per-type storage); fields still go to sol.
static int cached_method_index(lua_State* L) {
    // upvalue 1: cache table (method name -> function), upvalue 2: sol's __index
    lua_pushvalue(L, 2);
    if (lua_rawget(L, lua_upvalueindex(1)) != LUA_TNIL) {
        return 1;
    }
    lua_pop(L, 1);
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_call(L, 2, 1);
    if (lua_type(L, -1) == LUA_TFUNCTION && lua_type(L, 2) == LUA_TSTRING) {
        lua_pushvalue(L, 2);
        lua_pushvalue(L, -2);
        lua_rawset(L, lua_upvalueindex(1));
    }
    return 1;
}

// Wrap the C __index of every sol usertype metatable (registry keys "sol.*").
static void cache_usertype_method_lookups(lua_State* L) {
    lua_pushnil(L);
    while (lua_next(L, LUA_REGISTRYINDEX) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TTABLE &&
            std::strncmp(lua_tostring(L, -2), "sol.", 4) == 0) {
            lua_getfield(L, -1, "__index");
            if (lua_iscfunction(L, -1) && lua_tocfunction(L, -1) != cached_method_index) {
                lua_newtable(L);
                lua_insert(L, -2);
                lua_pushcclosure(L, cached_method_index, 2);
                lua_setfield(L, -2, "__index");
            } else {
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
}

void ScriptManager::register_lua_bindings() {
    sol::state& lua = *this->lua;
    // Calling an animation returns it: box:fade() and box.fade (a property) are the same object,
    // for skins written against either binding of the box animations.
    lua.new_usertype<BaseAnimation>(
        "BaseAnimation", sol::meta_function::call, [](BaseAnimation& self, sol::variadic_args) { return &self; },
        "update",
        [](BaseAnimation& self, double t) {
            self.update(t);
            return self.attribute;
        },
        "restart", &BaseAnimation::restart, "start", &BaseAnimation::start, "reset", &BaseAnimation::reset, "attribute",
        &BaseAnimation::attribute, "duration", &BaseAnimation::duration, "is_finished", &BaseAnimation::is_finished,
        "is_started", &BaseAnimation::is_started,
        // tex.get_animation() returns BaseAnimation*, so a MoveAnimation's start_position
        // must be reachable here (the C++ YellowBox does the same C-style cast).
        "start_position", sol::property([](BaseAnimation& self) -> int {
            if (auto* move = dynamic_cast<MoveAnimation*>(&self)) {
                return move->start_position;
            }
            return 0;
        }));

    // Fade animation bindings
    lua.new_usertype<FadeAnimation>(
        "FadeAnimation", sol::base_classes, sol::bases<BaseAnimation>(), sol::meta_function::call,
        [](FadeAnimation& self, sol::variadic_args) { return &self; }, "update",
        [](FadeAnimation& self, double t) {
            self.update(t);
            return self.attribute;
        },
        "restart", &FadeAnimation::restart);

    // Move animation bindings
    lua.new_usertype<MoveAnimation>(
        "MoveAnimation", sol::base_classes, sol::bases<BaseAnimation>(), sol::meta_function::call,
        [](MoveAnimation& self, sol::variadic_args) { return &self; }, "update",
        [](MoveAnimation& self, double t) {
            self.update(t);
            return self.attribute;
        },
        "restart", &MoveAnimation::restart, "start_position", &MoveAnimation::start_position);

    // Texture change animation bindings
    lua.new_usertype<TextureChangeAnimation>(
        "TextureChangeAnimation", sol::base_classes, sol::bases<BaseAnimation>(), sol::meta_function::call,
        [](TextureChangeAnimation& self, sol::variadic_args) { return &self; }, "update",
        [](TextureChangeAnimation& self, double t) {
            self.update(t);
            return self.attribute;
        },
        "reset", &TextureChangeAnimation::reset);

    // Text stretch animation bindings
    lua.new_usertype<TextStretchAnimation>(
        "TextStretchAnimation", sol::base_classes, sol::bases<BaseAnimation>(), sol::meta_function::call,
        [](TextStretchAnimation& self, sol::variadic_args) { return &self; }, "update",
        [](TextStretchAnimation& self, double t) {
            self.update(t);
            return self.attribute;
        });

    // Texture resize animation bindings
    lua.new_usertype<TextureResizeAnimation>(
        "TextureResizeAnimation", sol::base_classes, sol::bases<BaseAnimation>(), sol::meta_function::call,
        [](TextureResizeAnimation& self, sol::variadic_args) { return &self; }, "update",
        [](TextureResizeAnimation& self, double t) {
            self.update(t);
            return self.attribute;
        },
        "restart", &TextureResizeAnimation::restart);

    // Animation creation helper functions
    sol::table anim = lua.create_table();

    anim.set_function("fade", [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<FadeAnimation> {
        double initial_opacity = 1.0;
        double final_opacity = 0.0;
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;
        std::optional<EaseType> ease_in = std::nullopt;
        std::optional<EaseType> ease_out = std::nullopt;
        std::optional<double> reverse_delay = std::nullopt;

        if (params) {
            sol::table t = params.value();
            initial_opacity = t["initial_opacity"].get_or(initial_opacity);
            final_opacity = t["final_opacity"].get_or(final_opacity);
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);

            ease_in = parse_ease_type(t["ease_in"]);
            ease_out = parse_ease_type(t["ease_out"]);

            sol::optional<double> reverse_delay_opt = t["reverse_delay"];
            if (reverse_delay_opt) {
                reverse_delay = reverse_delay_opt.value();
            }
        }

        return std::make_unique<FadeAnimation>(duration, initial_opacity, loop, lock_input, final_opacity, delay,
                                               ease_in, ease_out, reverse_delay);
    });

    anim.set_function("move", [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<MoveAnimation> {
        int total_distance = 0;
        int start_position = 0;
        double delay = 0.0;
        bool loop = false;
        bool lock_input = false;
        std::optional<double> reverse_delay = std::nullopt;
        std::optional<EaseType> ease_in = std::nullopt;
        std::optional<EaseType> ease_out = std::nullopt;

        if (params) {
            sol::table t = params.value();
            total_distance = t["total_distance"].get_or(total_distance);
            start_position = t["start_position"].get_or(start_position);
            delay = t["delay"].get_or(delay);
            loop = t["loop"].get_or(loop);
            lock_input = t["lock_input"].get_or(lock_input);

            sol::optional<double> reverse_delay_opt = t["reverse_delay"];
            if (reverse_delay_opt) {
                reverse_delay = reverse_delay_opt.value();
            }

            ease_in = parse_ease_type(t["ease_in"]);
            ease_out = parse_ease_type(t["ease_out"]);
        }

        return std::make_unique<MoveAnimation>(duration, total_distance, loop, lock_input, start_position, delay,
                                               reverse_delay, ease_in, ease_out);
    });

    anim.set_function("texture_change",
                      [](double duration, sol::table textures_table,
                         sol::optional<sol::table> params) -> std::unique_ptr<TextureChangeAnimation> {
                          std::vector<std::tuple<double, double, int>> keyframes;

                          for (size_t i = 1; i <= textures_table.size(); ++i) {
                              sol::optional<sol::table> entry_opt = textures_table[i];
                              if (!entry_opt) {
                                  continue;
                              }
                              sol::table tex_entry = entry_opt.value();
                              sol::optional<double> start = tex_entry[1];
                              sol::optional<double> end = tex_entry[2];
                              sol::optional<int> index = tex_entry[3];
                              if (!start || !end || !index) {
                                  spdlog::error("anim.texture_change: malformed keyframe at index {}", i);
                                  continue;
                              }
                              keyframes.emplace_back(start.value(), end.value(), index.value());
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

    anim.set_function("text_stretch",
                      [](double duration, sol::optional<sol::table> params) -> std::unique_ptr<TextStretchAnimation> {
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

    lua["anim"] = anim;

    sol::table tex = lua.create_table();

    tex.set_function("get_animation", [](double anim_id, const sol::object& second_arg) -> BaseAnimation* {
        if (second_arg.get_type() == sol::type::string) {
            return ::tex.get_animation(static_cast<int>(anim_id), second_arg.as<std::string>());
        }
        // true: a private copy of the current screen's animation
        return ::tex.get_animation(static_cast<int>(anim_id), second_arg.is<bool>() && second_arg.as<bool>());
    });

    tex.set_function("load_folder", [](const std::string& screen_name, const std::string& subset) {
        script_manager.tex.load_folder(screen_name, subset);
    });

    tex.set_function("unload_folder", [](const std::string& screen_name, const std::string& subset) {
        script_manager.tex.unload_folder(screen_name, subset);
    });

    tex.set_function("current_screen", [] -> std::string { return global_data.current_screen; });

    tex.set_function("in_transition", [] -> bool { return global_data.in_transition; });

    tex.set_function("get_screen_width", [] -> float { return script_manager.tex.screen_width; });

    tex.set_function("get_screen_height", [] -> float { return script_manager.tex.screen_height; });

    tex.set_function("get_screen_scale", [] -> float { return script_manager.tex.screen_scale; });

    tex.set_function("get_draw_offset", [] -> std::pair<float, float> {
        return {script_manager.tex.draw_offset_x, script_manager.tex.draw_offset_y};
    });

    tex.set_function("draw_rect", [](float x, float y, float w, float h, double r, double g, double b, double a,
                                     sol::this_state state) {
        auto to_u8 = [](double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); };
        if (debug_log_draws) {
            debug_draw_log.push_back({"rect", {x, y, w, h}});
            log_lua_site(debug_draw_log.back(), state);
        }

        draw_rectangle({x, y, w, h}, Color{to_u8(r), to_u8(g), to_u8(b), to_u8(a)});
    });

    // rounded rectangle with an outline; roundness is raylib's 0..1 corner factor, the outline thickness is in pixels
    tex.set_function("draw_rounded_rect", [](float x, float y, float w, float h, float roundness,
                                             std::array<double, 4> fill, double thickness, std::array<double, 4> line) {
        auto to_u8 = [](double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); };
        Rect r{x, y, w, h};

        draw_rectangle_rounded(r, roundness, 16, Color{to_u8(fill[0]), to_u8(fill[1]), to_u8(fill[2]), to_u8(fill[3])});

        if (thickness > 0) {
            draw_rectangle_rounded_lines_ex(r, roundness, 16, static_cast<float>(thickness),
                                            Color{to_u8(line[0]), to_u8(line[1]), to_u8(line[2]), to_u8(line[3])});
        }
    });

    tex.set_function("begin_scissor", [](float x, float y, float w, float h) {
        int sx = virtual_to_screen_x(x);
        int ex = virtual_to_screen_x(x + w);
        int sy = virtual_to_screen_y(y);
        int ey = virtual_to_screen_y(y + h);
        begin_scissor_mode(sx, sy, ex - sx, ey - sy);
    });

    tex.set_function("end_scissor", [] { end_scissor_mode(); });

    tex.set_function("get_skin_config", [](const std::string& config_key) -> sol::optional<sol::table> {
        const SkinInfo* skin_ptr = font_manager.get_text_info(config_key);
        if (!skin_ptr) {
            auto config_it = script_manager.tex.skin_config_by_name.find(config_key);
            if (config_it == script_manager.tex.skin_config_by_name.end()) {
                return sol::nullopt;
            }
            skin_ptr = &config_it->second;
        }

        const auto& skin_info = *skin_ptr;

        sol::table info = script_manager.lua->create_table();
        info["x"] = skin_info.x;
        info["y"] = skin_info.y;
        info["font_size"] = skin_info.font_size;
        info["width"] = skin_info.width;
        info["height"] = skin_info.height;
        info["outline"] = skin_info.outline;

        return info;
    });

    tex.set_function("get_option", [](const std::string& name) -> bool {
        auto it = screen_options_map.find(name);
        if (it == screen_options_map.end()) {
            return false;
        }
        auto o = script_manager.tex.options.find(it->second);
        return o != script_manager.tex.options.end() && o->second;
    });

    tex.set_function("get_texture_keys", [](const std::string& subset) -> sol::optional<sol::table> {
        std::string prefix = subset + "/";
        sol::table keys = script_manager.lua->create_table();
        int index = 1;
        for (const auto& [path, obj] : script_manager.tex.textures) {
            if (path.size() > prefix.size() && path.starts_with(prefix)) {
                keys[index] = path.substr(prefix.size());
                ++index;
            }
        }
        if (index == 1) {
            return sol::nullopt;
        }
        return keys;
    });

    tex.set_function("get_texture_info",
                     [](const std::string& subset, const std::string& texture_name) -> sol::optional<sol::table> {
                         auto tex_it = script_manager.tex.textures.find(subset + "/" + texture_name);
                         if (tex_it == script_manager.tex.textures.end()) {
                             return sol::nullopt;
                         }

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
                         if (auto* framed = dynamic_cast<FramedTexture*>(tex_obj.get())) {
                             frame_count = static_cast<int>(framed->textures.size());
                         }
                         if (tex_obj->crop_data.has_value()) {
                             frame_count = std::max(frame_count, static_cast<int>(tex_obj->crop_data->size()));
                         }
                         info["frame_count"] = frame_count;

                         return info;
                     });

    tex.set_function("draw_texture", [](const std::shared_ptr<TextureObject>& id,
                                        const sol::optional<sol::table>& params_table, sol::this_state state) {
        if (!id) {
            DrawLogEntry entry{"null_texture", {}};
            log_lua_site(entry, state);
            if (entry.from_lua) {
                spdlog::warn("draw_texture: null texture (called from {}:{} in {})", entry.lua_source, entry.lua_line,
                             entry.lua_function.empty() ? "?" : entry.lua_function);
            } else {
                spdlog::warn("draw_texture: null texture");
            }
            return;
        }
        script_manager.tex.draw_texture(id.get(), parse_draw_params(params_table));
    });

    tex.set_function("get_texture", [](const std::string& path) -> sol::optional<std::shared_ptr<TextureObject>> {
        TextureWrapper& w = script_manager.tex;
        const std::string& lang = global_data.config->general.language;
        if (w.lua_texture_lookup_lang != lang) {
            w.lua_texture_lookup.clear();
            w.lua_texture_lookup_lang = lang;
        }
        if (auto it = w.lua_texture_lookup.find(path); it != w.lua_texture_lookup.end()) {
            if (it->second) {
                return it->second;
            }
            return sol::nullopt;
        }
        // resolve_lua_texture may load the folder, which empties the lookup: store afterwards
        sol::optional<std::shared_ptr<TextureObject>> found = resolve_lua_texture(path);
        w.lua_texture_lookup[path] = found ? *found : nullptr;
        return found;
    });

    tex.set_function("language", [] { return global_data.config->general.language; });
    tex.set_function(
        "skin_text", [](const std::string& key, const sol::optional<std::string>& lang_opt) -> std::string {
            const SkinInfo* info = font_manager.get_text_info(key);
            if (!info) {
                return "";
            }
            const auto& m = info->text;
            for (const std::string& l :
                 {lang_opt.value_or(global_data.config->general.language), std::string("ja"), std::string("en")}) {
                auto t = m.find(l);
                if (t != m.end() && !t->second.empty()) {
                    return t->second;
                }
            }
            return m.empty() ? std::string() : m.begin()->second;
        });

    tex.set_function("skip_enabled", [] { return global_data.live_skip_count >= 0; });

    lua["tex"] = tex;

    lua.new_usertype<OutlinedText>("OutlinedText", "width", &OutlinedText::width, "height", &OutlinedText::height,
                                   "text", sol::property([](OutlinedText& self) { return self.get_text(); }), "draw",
                                   [](OutlinedText& self, const sol::optional<sol::table>& params_table) {
                                       DrawTextureParams params = parse_draw_params(params_table);
                                       self.draw(params);
                                   });

    sol::table text = lua.create_table();
    text.set_function("create_text",
                      [](const std::string& skin_config_key, std::array<double, 4> color,
                         std::array<double, 4> outline_color, bool is_vertical, float outline_thickness, float spacing,
                         const sol::optional<std::string>& font) -> std::unique_ptr<OutlinedText> {
                          const SkinInfo* cfg = font_manager.get_text_info(skin_config_key);
                          std::string text;
                          if (cfg) {
                              text = font_manager.get_text_label(skin_config_key);
                          } else { // a position-only skin_config entry draws empty text
                              cfg = [&] -> const SkinInfo* {
                                  auto it = script_manager.tex.skin_config_by_name.find(skin_config_key);
                                  return it == script_manager.tex.skin_config_by_name.end() ? nullptr : &it->second;
                              }();
                              if (!cfg) {
                                  spdlog::error("Skin config key not found: {}", skin_config_key);
                                  throw sol::error("Skin config key not found: " + skin_config_key);
                              }
                          }
                          int font_size = cfg->font_size;
                          auto to_u8 = [](double v) {
                              return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255));
                          };
                          Color color_val;
                          color_val.r = to_u8(color[0]);
                          color_val.g = to_u8(color[1]);
                          color_val.b = to_u8(color[2]);
                          color_val.a = to_u8(color[3]);
                          Color outline_color_val;
                          outline_color_val.r = to_u8(outline_color[0]);
                          outline_color_val.g = to_u8(outline_color[1]);
                          outline_color_val.b = to_u8(outline_color[2]);
                          outline_color_val.a = to_u8(outline_color[3]);
                          std::unique_ptr<OutlinedText> ptr =
                              std::make_unique<OutlinedText>(text, font_size, color_val, outline_color_val, is_vertical,
                                                             outline_thickness, spacing, 1.0f, font.value_or(""));
                          ptr->x_offset = cfg->x;
                          ptr->y_offset = cfg->y;
                          return ptr;
                      });

    text.set_function(
        "create_raw_text",
        [](const std::string& content, double font_size, std::array<double, 4> color,
           std::array<double, 4> outline_color, bool is_vertical, sol::optional<float> thickness,
           sol::optional<float> spacing, sol::optional<float> v_advance,
           const sol::optional<std::string>& font) -> std::unique_ptr<OutlinedText> {
            auto to_u8 = [](double v) { return static_cast<uint8_t>(std::clamp(static_cast<int>(v), 0, 255)); };
            Color c = {to_u8(color[0]), to_u8(color[1]), to_u8(color[2]), to_u8(color[3])};
            Color oc = {to_u8(outline_color[0]), to_u8(outline_color[1]), to_u8(outline_color[2]),
                        to_u8(outline_color[3])};
            return std::make_unique<OutlinedText>(content, static_cast<int>(font_size), c, oc, is_vertical,
                                                  thickness.value_or(5.0f), spacing.value_or(2.0f),
                                                  v_advance.value_or(1.0f), font.value_or(""));
        });

    lua["text"] = text;

    tex.set_function("get_current_ms",
                     [] -> double { return script_manager.current_ms_override.value_or(get_current_ms()); });

    tex.set_function("player_num", [] -> int { return static_cast<int>(global_data.player_num); });

    tex.set_function("display_bpm", [] -> bool { return global_data.config->general.display_bpm; });

    tex.set_function("timing_offset", [](int player_num) -> sol::optional<std::string> {
        if (!global_data.config->general.show_timing_offset) {
            return sol::nullopt;
        }
        const ResultData& r = global_data.session_data[player_num].result_data;
        if (r.hit_offset_count <= 0) {
            return std::string("-- ms");
        }
        const long ms = std::lround(r.hit_offset_sum_ms / r.hit_offset_count);
        return (ms > 0 ? "+" : "") + std::to_string(ms) + " ms";
    });

    tex.set_function("song_title", [] -> std::string { return current_session().song_title; });

    tex.set_function("song_subtitle", [] -> std::string { return current_session().song_subtitle; });

    tex.set_function("genre_frame", [](double genre_index) -> int {
        return genre_to_ref_frame(static_cast<GenreIndex>(static_cast<int>(genre_index)));
    });

    tex.set_function("songs_played", [] -> int { return global_data.songs_played; });

    tex.set_function("song_limit", [] -> int { return global_data.config->limits.song_limit; });

    tex.set_function("last_input_ms", [] -> double { return get_last_input_ms(); });

    tex.set_function("log", [](const std::string& msg) { spdlog::info("[lua] {}", msg); });

    sol::table audio_tbl = lua.create_table();

    audio_tbl.set_function("play_sound", [](const std::string& name, sol::optional<std::string> preset_str,
                                            sol::optional<bool> loop, sol::optional<int> player) {
        struct NamedPreset {
            std::string_view name;
            VolumePreset preset;
        };

        static constexpr NamedPreset presets[] = {
            {"sound", VolumePreset::SOUND},
            {"music", VolumePreset::MUSIC},
            {"voice", VolumePreset::VOICE},
            {"hitsound", VolumePreset::HITSOUND},
            {"attract_mode", VolumePreset::ATTRACT_MODE},
        };

        VolumePreset preset = VolumePreset::NONE;
        if (preset_str) {
            const auto* const match = std::ranges::find(presets, preset_str.value(), &NamedPreset::name);
            if (match != std::end(presets)) {
                preset = match->preset;
            }
        }
        PlayerSoundScope audio_owner(preset == VolumePreset::MUSIC
                                         ? PlayerNum::ALL
                                         : static_cast<PlayerNum>(player.value_or(PlayerSoundScope::player())));
        audio.set_sound_loop(name, loop.value_or(is_bgm_sound(name)));
        audio.play_sound(name, preset);
    });

    audio_tbl.set_function("stop_sound", [](const std::string& name) {
        audio.set_sound_loop(name, false);
        audio.stop_sound(name);
    });

    audio_tbl.set_function("is_sound_playing",
                           [](const std::string& name) -> bool { return audio.is_sound_playing(name); });

    audio_tbl.set_function("play_count",
                           [](const std::string& name) -> double { return audio.sound_play_count(name); });

    audio_tbl.set_function("has_sound", [](const std::string& name) -> bool { return audio.has_sound(name); });

    lua["audio"] = audio_tbl;

    sol::table camera_tbl = lua.create_table();

    camera_tbl.set_function("open", [] -> bool { return webcam.open(global_data.config->general.webcam_number); });

    camera_tbl.set_function("close", [] { webcam.close(); });

    camera_tbl.set_function("update", [] { webcam.update(); });

    camera_tbl.set_function("draw", [](float x, float y, float w, float h) {
        if (!webcam.is_ready()) {
            return;
        }
        Rect src{0, 0, static_cast<float>(webcam.width()), static_cast<float>(webcam.height())};
        Rect dst{x, y, w, h};
        draw_texture_pro(webcam.get_texture(), src, dst, Vec2{0, 0}, 0, Color::WHITE);
    });

    lua["camera"] = camera_tbl;

    register_song_select_lua_bindings(lua);

    cache_usertype_method_lookups(lua.lua_state());
}

// sol creates some metatables lazily (e.g. the unique_ptr variant the first time an anim.fade()
// object is pushed), so new ones are wrapped after script loads and once a second.
void ScriptManager::refresh_method_cache() {
    if (lua) {
        cache_usertype_method_lookups(lua->lua_state());
    }
}

ScriptManager script_manager;
