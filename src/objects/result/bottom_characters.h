#pragma once

#include "../../libs/animation.h"
#include "../enums.h"

class BottomCharacters {
private:

    MoveAnimation* move_up;
    MoveAnimation* move_down;
    MoveAnimation* bounce_up;
    MoveAnimation* bounce_down;
    MoveAnimation* move_center;
    MoveAnimation* c_bounce_up;
    MoveAnimation* c_bounce_down;
    MoveAnimation* flower_up;

    std::optional<ResultState> state;
    int flower_index = 0;
    std::optional<double> flower_start;
    int chara_0_index = 0;
    int chara_1_index = 0;

    void draw_flowers();

public:

    BottomCharacters();

    void start();

    void update(ResultState state);

    void draw();

    bool is_finished();
};
