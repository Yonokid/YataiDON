#include "background.h"

Background::Background(PlayerNum player_num, float bpm, const std::string& scene_preset) {
    L = luaL_newstate();
    luaL_openlibs(L);

    luaL_newmetatable(L, "BaseAnimation");
    lua_pushcfunction(L, lua_animation_index);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, lua_draw_texture);
    lua_setfield(L, -2, "draw_texture");
    lua_pushcfunction(L, lua_load_folder);
    lua_setfield(L, -2, "load_folder");
    lua_pushcfunction(L, lua_load_animations);
    lua_setfield(L, -2, "load_animations");
    lua_pushcfunction(L, lua_get_animation);
    lua_setfield(L, -2, "get_animation");
    lua_pushcfunction(L, lua_get_texture_info);
    lua_setfield(L, -2, "get_texture_info");
    lua_setglobal(L, "tex");

    std::string script_path = script_manager.get_lua_script_path("background");
    if (luaL_dofile(L, script_path.c_str()) != LUA_OK) {
        spdlog::error("Error loading background.lua: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_getglobal(L, "Background");
    lua_getfield(L, -1, "new");
    lua_pushinteger(L, static_cast<int>(player_num));
    lua_pushnumber(L, bpm);
    lua_pushstring(L, scene_preset.c_str());

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        spdlog::error("Error calling Background.new: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
}

Background::~Background() {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "destroy");
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, -2);  // push self
        lua_pcall(L, 1, 0, 0);
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    luaL_unref(L, LUA_REGISTRYINDEX, lua_ref);
    lua_close(L);
}

void Background::update(double current_ms) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "update");
    lua_pushvalue(L, -2);  // push self
    lua_pushnumber(L, current_ms);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        spdlog::error("Error calling update: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1); // pop self
}

void Background::handle_good(PlayerNum player_num) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "handle_good");
    lua_pushvalue(L, -2);  // push self
    lua_pushnumber(L, (int)player_num);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        spdlog::error("Error calling handle_good: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void Background::handle_ok(PlayerNum player_num) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "handle_ok");
    lua_pushvalue(L, -2);  // push self
    lua_pushnumber(L, (int)player_num);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        spdlog::error("Error calling handle_ok: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void Background::handle_bad(PlayerNum player_num) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "handle_bad");
    lua_pushvalue(L, -2);  // push self
    lua_pushnumber(L, (int)player_num);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        spdlog::error("Error calling handle_bad: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void Background::handle_drumroll(PlayerNum player_num) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "handle_drumroll");
    lua_pushvalue(L, -2);  // push self
    lua_pushnumber(L, (int)player_num);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        spdlog::error("Error calling handle_drumroll: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void Background::handle_balloon(PlayerNum player_num) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "handle_balloon");
    lua_pushvalue(L, -2);  // push self
    lua_pushnumber(L, (int)player_num);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        spdlog::error("Error calling handle_balloon: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void Background::draw_back() {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "draw_back");
    lua_pushvalue(L, -2);  // push self

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        spdlog::error("Error calling draw_back: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void Background::draw_fore() {
    lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref);
    lua_getfield(L, -1, "draw_fore");
    lua_pushvalue(L, -2);  // push self

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        spdlog::error("Error calling draw_fore: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}
