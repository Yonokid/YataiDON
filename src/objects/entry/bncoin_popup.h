#pragma once

#include <string>

#include "../../libs/file/script.h"

class BanaCoinPopup : public LuaScript {
public:
    enum Action {
        NONE = 0,
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };

    BanaCoinPopup(const std::string& kind, int balance, int cost);
    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    bool input(Action action); // false: ignored
    void draw();
    bool is_done();
    bool paid(); // practice: the player paid `cost`

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_get_result;
};
