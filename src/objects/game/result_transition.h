#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"

class ResultTransition {
private:
    PlayerNum player_num;
    MoveAnimation* move;

public:
    bool is_finished;
    bool is_started;

    ResultTransition() = default;

    ResultTransition(PlayerNum player_num);

    void start();
    void update(double current_ms);
    void draw();
};
