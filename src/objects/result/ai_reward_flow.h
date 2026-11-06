#pragma once

#include "../../libs/file/script.h"
#include "../../libs/network/network.h"

class AIRewardFlow : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;

public:
    enum Action {
        KATSU = 1,
        DON = 2
    };

    explicit AIRewardFlow(const AIBattleProgress& progress);
    bool unavailable() const { return !fn_update.valid(); }
    void update(double current_ms);
    bool input(Action action);
    void draw();
    bool is_done();
};
