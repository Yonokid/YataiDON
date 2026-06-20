#pragma once

#include "../../libs/script.h"

class ResultCrownMessage : public LuaScript {
    sol::protected_function fn_update, fn_draw;
public:
    ResultCrownMessage() = default;
    ResultCrownMessage(int frame, bool is_2p);
    void update(double current_ms);
    void draw();
};
