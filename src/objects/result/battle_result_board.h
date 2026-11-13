#pragma once

#include "../../libs/file/script.h"
#include "../../libs/global_data.h"
#include "../../libs/network/network.h"
#include "../global/nameplate.h"

class BattleResultBoard : public LuaScript {
    sol::protected_function fn_begin;
    sol::protected_function fn_update;
    sol::protected_function fn_decide;
    sol::protected_function fn_draw_background;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_nameplate_pos;
    Nameplate nameplate;

public:
    BattleResultBoard(const BattleTally& song, int song_number, int bond);
    bool unavailable() const { return !fn_update.valid(); }
    void begin(const BattleProgress& progress);
    void update(double current_ms);
    bool decide();
    void draw_background();
    void draw();
    bool is_done();
};
