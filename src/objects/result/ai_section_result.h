#pragma once

#include "../../libs/file/script.h"
#include "../../libs/global_data.h"
#include "../../libs/network/network.h"

class AISectionBoard : public LuaScript {
    sol::protected_function fn_begin;
    sol::protected_function fn_update;
    sol::protected_function fn_decide;
    sol::protected_function fn_toggle;
    sol::protected_function fn_draw_background;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;

public:
    explicit AISectionBoard(const AIMatchResult& match);
    bool unavailable() const { return !fn_update.valid(); }
    void begin(const AIBattleProgress& progress);
    void update(double current_ms);
    bool decide();
    bool toggle();
    void draw_background();
    void draw();
    bool is_done();
};
