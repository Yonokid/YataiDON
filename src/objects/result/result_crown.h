#pragma once

#include "../../libs/script.h"

enum CrownType {
    CROWN_CLEAR = 0,
    CROWN_FC    = 1,
    CROWN_DFC   = 2
};

class ResultCrown : public LuaScript {
    sol::protected_function fn_update, fn_draw, fn_is_settled;
public:
    ResultCrown() = default;
    ResultCrown(int crown_type, bool is_2p);
    void update(double current_ms);
    void draw();
    bool is_settled();
};
