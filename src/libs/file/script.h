#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

#include "../gfx/texture.h"
#include "../profiler.h"

class LuaScript {
public:
    virtual ~LuaScript() = default;

protected:
    sol::table lua_object;

    template<typename... Args>
    bool load(const std::string& class_name, const std::string& script_name, Args&&... args);

    template<typename... Args>
    void call(sol::protected_function& fn, std::string_view context, Args&&... args) {
        if (!fn.valid()) {
            return;
        }
        PROFILE_SCOPE_N("lua");
        PROFILE_ZONE_NAME(context);
        auto result = fn(lua_object, std::forward<Args>(args)...);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Lua error in {}: {}", context, err.what());
        }
    }

    template<typename Ret, typename... Args>
    sol::optional<Ret> call_r(sol::protected_function& fn, std::string_view context, Args&&... args) {
        if (!fn.valid()) {
            return sol::nullopt;
        }
        PROFILE_SCOPE_N("lua");
        PROFILE_ZONE_NAME(context);
        auto result = fn(lua_object, std::forward<Args>(args)...);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Lua error in {}: {}", context, err.what());
            return sol::nullopt;
        }
        try {
            return result.template get<Ret>();
        } catch (const std::exception& e) {
            spdlog::error("Lua error in {} (bad return value): {}", context, e.what());
            return sol::nullopt;
        }
    }
};

class ScriptManager {
private:
    std::map<std::string, std::string> scripts;
    std::set<std::string> executed_scripts;

public:
    TextureWrapper& tex;
    std::unique_ptr<sol::state> lua;
    std::optional<double> current_ms_override;

    ScriptManager() : tex(::tex) {}

    void init(const fs::path& script_path);
    bool has_lua_script(const std::string& script_name) const;
    void shutdown();
    std::string get_lua_script_path(const std::string& script_name);
    void index_scripts(const fs::path& script_path);
    void register_lua_bindings();
    void refresh_method_cache();

    bool script_executed(const std::string& script_name) const { return executed_scripts.contains(script_name); }
    void mark_script_executed(const std::string& script_name) { executed_scripts.insert(script_name); }
    void collect_garbage();
};

extern ScriptManager script_manager;

inline lua_State* script_lua_state() {
    return script_manager.lua ? script_manager.lua->lua_state() : nullptr;
}

void log_lua_site(DrawLogEntry& entry, lua_State* state);

template<typename... Args>
bool LuaScript::load(const std::string& class_name, const std::string& script_name, Args&&... args) {
    if (!script_manager.lua) {
        return false;
    }
    PROFILE_SCOPE_N("lua load");
    PROFILE_ZONE_TEXT(script_name);
    sol::state& lua = *script_manager.lua;

    if (!script_manager.script_executed(script_name)) {
        // A skin that scripts some screens but not this one simply has no
        // script here; that is a plain "not scripted", not an error.
        if (!script_manager.has_lua_script(script_name)) {
            return false;
        }
        auto result = lua.script_file(script_manager.get_lua_script_path(script_name));
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Error loading {}.lua: {}", script_name, err.what());
            return false;
        }
        script_manager.mark_script_executed(script_name);
    }

    if (!lua[class_name].valid()) {
        spdlog::error("{}.lua loaded but does not define class {}", script_name, class_name);
        return false;
    }

    sol::protected_function new_func = lua[class_name]["new"];
    if (!new_func.valid()) {
        spdlog::error("{}.new is not a defined function for class {}", script_name, class_name);
        return false;
    }
    auto call_result = new_func(std::forward<Args>(args)...);
    if (!call_result.valid()) {
        sol::error err = call_result;
        spdlog::error("Error calling {}.new: {}", class_name, err.what());
        return false;
    }

    lua_object = call_result;
    script_manager.refresh_method_cache();
    return true;
}
