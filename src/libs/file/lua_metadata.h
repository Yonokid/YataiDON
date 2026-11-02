#pragma once

#include <lua.hpp>
#include <span>
#include <string_view>

namespace lua_metadata {
    inline void number(lua_State* state, const char* key, lua_Number value) {
        lua_pushnumber(state, value);
        lua_setfield(state, -2, key);
    }

    inline void integer(lua_State* state, const char* key, lua_Integer value) {
        lua_pushinteger(state, value);
        lua_setfield(state, -2, key);
    }

    inline void array(lua_State* state, const char* key, std::span<const int> values) {
        lua_createtable(state, static_cast<int>(values.size()), 0);
        for (size_t i = 0; i < values.size(); ++i) {
            lua_pushinteger(state, values[i]);
            lua_rawseti(state, -2, static_cast<lua_Integer>(i + 1));
        }
        lua_setfield(state, -2, key);
    }

    struct TextureInfo {
        std::string_view name;
        std::span<const int> x, y, x2, y2;
        int width, height, frames;
    };

    // Each call still returns independent writable Lua tables and coordinate
    // arrays; only the C++ proxy/conversion overhead is removed.
    inline void push_texture(lua_State* state, const TextureInfo& info) {
        lua_createtable(state, 0, 8);
        lua_pushlstring(state, info.name.data(), info.name.size());
        lua_setfield(state, -2, "name");
        array(state, "x", info.x);
        array(state, "y", info.y);
        array(state, "x2", info.x2);
        array(state, "y2", info.y2);
        integer(state, "width", info.width);
        integer(state, "height", info.height);
        integer(state, "frame_count", info.frames);
    }

    struct SkinInfo {
        float x, y;
        int font_size;
        float width, height, outline;
    };

    inline void push_skin(lua_State* state, const SkinInfo& info) {
        lua_createtable(state, 0, 6);
        number(state, "x", info.x);
        number(state, "y", info.y);
        integer(state, "font_size", info.font_size);
        number(state, "width", info.width);
        number(state, "height", info.height);
        number(state, "outline", info.outline);
    }
} // namespace lua_metadata
