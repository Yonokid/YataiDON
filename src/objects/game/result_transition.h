#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"
#include "../../libs/script.h"

class ResultTransition : public LuaScript {
private:
    PlayerNum player_num;
    MoveAnimation* move;

    sol::protected_function fn_start, fn_update, fn_draw, fn_is_finished;

    void draw_default();

public:
    bool is_finished;
    bool is_started;

    ResultTransition() = default;

    ResultTransition(PlayerNum player_num);

    void start();
    void update(double current_ms);
    void draw();
};
