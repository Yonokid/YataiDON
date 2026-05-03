#pragma once

#include "../../libs/global_data.h"

class ResultBackground {
private:
    PlayerNum player_num;
    float width;
public:

    ResultBackground() = default;
    ResultBackground(PlayerNum player_num, float width);

    void draw();
};
