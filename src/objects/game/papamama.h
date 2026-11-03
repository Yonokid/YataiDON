#pragma once

#include "../../libs/file/script.h"

class PapamamaPopup : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;

public:
    PapamamaPopup();
    void update(double current_ms);
    void draw();
};
