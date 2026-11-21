#pragma once

#include "../../libs/file/script.h"

class FCAnimation : public LuaScript {
private:
    sol::protected_function fn_update, fn_draw;

public:
    FCAnimation(bool is_2p, bool donderful = false, bool two_player = false);

    void update(double current_ms);
    void draw();
};
