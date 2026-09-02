#pragma once

#include "texture.h"
#include <sol/sol.hpp>
#include <chrono>
#include <spdlog/spdlog.h>

class LuaScript {
protected:
    sol::table lua_object;

    template<typename... Args>
    bool load(const std::string& class_name, const std::string& script_name, Args&&... args);

    template<typename... Args>
    void call(sol::protected_function& fn, const std::string& context, Args&&... args) {
        if (!fn.valid()) return;
        auto result = fn(lua_object, std::forward<Args>(args)...);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Lua error in {}: {}", context, err.what());
        }
    }

    template<typename Ret, typename... Args>
    sol::optional<Ret> call_r(sol::protected_function& fn, const std::string& context, Args&&... args) {
        if (!fn.valid()) return sol::nullopt;
        auto result = fn(lua_object, std::forward<Args>(args)...);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Lua error in {}: {}", context, err.what());
            return sol::nullopt;
        }
        return result.template get<Ret>();
    }
};

class ScriptManager {
private:
    std::map<std::string, std::string> scripts;
public:
    TextureWrapper tex;
    std::unique_ptr<sol::state> lua;

    void init(fs::path script_path);
    bool has_lua_script(const std::string& script_name) const;
    void shutdown();
    std::string get_lua_script_path(const std::string& script_name);
    void index_scripts(const fs::path& script_path);
    void register_lua_bindings();
};

extern ScriptManager script_manager;

template<typename... Args>
bool LuaScript::load(const std::string& class_name, const std::string& script_name, Args&&... args) {
    if (!script_manager.lua) return false;
    sol::state& lua = *script_manager.lua;

    if (!lua[class_name].valid()) {
        // A skin that scripts some screens but not this one simply has no
        // script here; that is a plain "not scripted", not an error.
        if (!script_manager.has_lua_script(script_name)) return false;
        auto result = lua.script_file(script_manager.get_lua_script_path(script_name));
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Error loading {}.lua: {}", script_name, err.what());
            return false;
        }
    }

    sol::protected_function new_func = lua[class_name]["new"];
    auto call_result = new_func(std::forward<Args>(args)...);
    if (!call_result.valid()) {
        sol::error err = call_result;
        spdlog::error("Error calling {}.new: {}", class_name, err.what());
        return false;
    }

    lua_object = call_result;
    return true;
}
