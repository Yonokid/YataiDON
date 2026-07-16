#pragma once

#include "../../libs/script.h"

class EntryOverlay : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
public:
    EntryOverlay();
    bool online = false;
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
