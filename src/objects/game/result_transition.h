#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"

class ResultTransition {
private:
    PlayerNum player_num;
    MoveAnimation* move;

public:
    bool is_finished;
    bool is_started;

    ResultTransition(PlayerNum player_num);

    void start();
    void update(double current_ms);
    void draw();
};
