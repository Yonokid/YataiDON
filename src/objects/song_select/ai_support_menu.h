#pragma once

#include "../../libs/file/script.h"

class AISupportMenu : public LuaScript {
    sol::protected_function fn_draw;

public:
    AISupportMenu();
    bool loaded() const { return lua_object.valid(); }
    void draw(double ms, int course, int cursor, bool choosing, bool branching, int player, int tension);
};
