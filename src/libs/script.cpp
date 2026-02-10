#include "script.h"

void ScriptManager::init(fs::path script_path) {
    for (const auto& script : fs::directory_iterator(script_path)) {
        scripts[script.path().stem().string()] = script.path();
    }
    spdlog::debug("Loaded scripts:");
    for (const auto& [name, path] : scripts) {
        spdlog::debug("  {} -> {}", name, path);
    }
    spdlog::debug("Total scripts: {}", scripts.size());

    tex.init(script_path.parent_path() / "Graphics");
}

std::string ScriptManager::get_lua_script_path(const std::string& script_name) {
    if (scripts.find(script_name) == scripts.end()) {
        throw std::runtime_error("Unable to find lua script: " + script_name);
    }
    return scripts[script_name];
}

BaseAnimation** check_animation(lua_State* L, int index) {
    return (BaseAnimation**)luaL_checkudata(L, index, "BaseAnimation");
}

int lua_animation_update(lua_State* L) {
    BaseAnimation** anim = check_animation(L, 1);
    double current_ms = luaL_checknumber(L, 2);

    (*anim)->update(current_ms);
    return 0;
}

int lua_animation_index(lua_State* L) {
    BaseAnimation** anim = check_animation(L, 1);
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "update") == 0) {
        lua_pushcfunction(L, lua_animation_update);
        return 1;
    }

    if (strcmp(key, "attribute") == 0) {
        lua_pushnumber(L, (*anim)->attribute);
        return 1;
    }

    return 0;
}

int lua_load_animations(lua_State* L) {
    const char* screen_name = luaL_checkstring(L, 1);
    script_manager.tex.load_animations(screen_name);
    return 0;
}

int lua_get_animation(lua_State* L) {
    int anim_id = luaL_checkinteger(L, 1);
    bool is_copy = false;

    if (lua_gettop(L) >= 2) {
        is_copy = lua_toboolean(L, 2);
    }

    BaseAnimation* anim = script_manager.tex.get_animation(anim_id, is_copy);

    BaseAnimation** udata = (BaseAnimation**)lua_newuserdata(L, sizeof(BaseAnimation*));
    *udata = anim;

    luaL_getmetatable(L, "BaseAnimation");
    lua_setmetatable(L, -2);

    return 1;
}

int lua_load_folder(lua_State* L) {
    const char* screen_name = luaL_checkstring(L, 1);
    const char* subset = luaL_checkstring(L, 2);

    script_manager.tex.load_folder(screen_name, subset);
    return 0;
}

int lua_get_texture_info(lua_State* L) {
    const char* subset = luaL_checkstring(L, 1);
    const char* texture_name = luaL_checkstring(L, 2);

    auto subset_it = script_manager.tex.textures.find(subset);
    if (subset_it == script_manager.tex.textures.end()) {
        lua_pushnil(L);
        return 1;
    }

    auto texture_it = subset_it->second.find(texture_name);
    if (texture_it == subset_it->second.end()) {
        lua_pushnil(L);
        return 1;
    }

    const auto& tex_obj = texture_it->second;

    lua_newtable(L);

    lua_pushstring(L, tex_obj->name.c_str());
    lua_setfield(L, -2, "name");

    lua_pushinteger(L, tex_obj->width);
    lua_setfield(L, -2, "width");

    lua_pushinteger(L, tex_obj->height);
    lua_setfield(L, -2, "height");

    return 1;
}

int lua_draw_texture(lua_State* L) {
    const char* subset = luaL_checkstring(L, 1);
    const char* texture_name = luaL_checkstring(L, 2);

    DrawTextureParams params;

    // If a third argument (table) is provided, parse it
    if (lua_gettop(L) >= 3 && lua_istable(L, 3)) {
        // color (as table {r, g, b, a})
        lua_getfield(L, 3, "color");
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            params.color.r = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_rawgeti(L, -1, 2);
            params.color.g = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_rawgeti(L, -1, 3);
            params.color.b = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_rawgeti(L, -1, 4);
            params.color.a = lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        // frame
        lua_getfield(L, 3, "frame");
        if (lua_isnumber(L, -1)) {
            params.frame = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // scale
        lua_getfield(L, 3, "scale");
        if (lua_isnumber(L, -1)) {
            params.scale = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        // center
        lua_getfield(L, 3, "center");
        if (lua_isboolean(L, -1)) {
            params.center = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        // mirror
        lua_getfield(L, 3, "mirror");
        if (lua_isstring(L, -1)) {
            params.mirror = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        // x, y
        lua_getfield(L, 3, "x");
        if (lua_isnumber(L, -1)) {
            params.x = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "y");
        if (lua_isnumber(L, -1)) {
            params.y = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        // x2, y2
        lua_getfield(L, 3, "x2");
        if (lua_isnumber(L, -1)) {
            params.x2 = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 3, "y2");
        if (lua_isnumber(L, -1)) {
            params.y2 = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        // origin (as table {x, y})
        lua_getfield(L, 3, "origin");
        if (lua_istable(L, -1)) {
            lua_rawgeti(L, -1, 1);
            params.origin.x = lua_tonumber(L, -1);
            lua_pop(L, 1);

            lua_rawgeti(L, -1, 2);
            params.origin.y = lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        // rotation
        lua_getfield(L, 3, "rotation");
        if (lua_isnumber(L, -1)) {
            params.rotation = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        // fade
        lua_getfield(L, 3, "fade");
        if (lua_isnumber(L, -1)) {
            params.fade = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

        // index
        lua_getfield(L, 3, "index");
        if (lua_isnumber(L, -1)) {
            params.index = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        // src (as table {x, y, width, height})
        lua_getfield(L, 3, "src");
        if (lua_istable(L, -1)) {
            ray::Rectangle rect;
            lua_getfield(L, -1, "x");
            rect.x = lua_tonumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "y");
            rect.y = lua_tonumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "width");
            rect.width = lua_tonumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "height");
            rect.height = lua_tonumber(L, -1);
            lua_pop(L, 1);

            params.src = rect;
        }
        lua_pop(L, 1);

        // controllable
        lua_getfield(L, 3, "controllable");
        if (lua_isboolean(L, -1)) {
            params.controllable = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);
    }

    script_manager.tex.draw_texture(subset, texture_name, params);
    return 0;
}

ScriptManager script_manager;
