#pragma once

#include "../../libs/file/script.h"

class Toast : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
    sol::protected_function fn_show;

public:
    Toast();
    bool available = false;
    void update(double current_ms);
    void draw();
};
