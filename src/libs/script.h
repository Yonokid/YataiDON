#pragma once

#include "global_data.h"
#include <spdlog/spdlog.h>
#include <lua.hpp>
#include "texture.h"

class ScriptManager {
private:
    std::map<std::string, std::string> scripts;

public:
    TextureWrapper tex;

    void init(fs::path script_path);

    std::string get_lua_script_path(const std::string& script_name);
};

BaseAnimation** check_animation(lua_State* L, int index);

int lua_animation_update(lua_State* L);

int lua_animation_index(lua_State* L);

int lua_load_folder(lua_State* L);

int lua_load_animations(lua_State* L);

int lua_get_animation(lua_State* L);

int lua_get_texture_info(lua_State* L);

int lua_draw_texture(lua_State* L);

extern ScriptManager script_manager;
