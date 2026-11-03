#pragma once

#include "../../libs/file/script.h"

class PracticeHelp : public LuaScript {
public:
    enum Action {
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };

    explicit PracticeHelp(bool is_2p);
    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    bool input(Action action); // false: ignored
    void draw();
    bool is_done();

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
};
