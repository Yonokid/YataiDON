#pragma once

#include "../../libs/file/script.h"

class AIIntro : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;

public:
    enum Action {
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };

    AIIntro();
    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    void input(Action action);
    void draw();
    bool is_done();
};
