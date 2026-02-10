#pragma once
#include "global_data.h"
#include <spdlog/spdlog.h>
#include <sol/sol.hpp>
#include "texture.h"

class ScriptManager {
private:
    std::map<std::string, std::string> scripts;
public:
    TextureWrapper tex;
    sol::state lua;

    void init(fs::path script_path);
    std::string get_lua_script_path(const std::string& script_name);
    void register_lua_bindings();
};

extern ScriptManager script_manager;
