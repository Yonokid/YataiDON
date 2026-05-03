#pragma once

#include "../../libs/global_data.h"
#include "../../libs/animation.h"

class FadeIn {
public:
    FadeIn() = default;
    FadeIn(PlayerNum player_num);
    void update(double current_ms);
    void draw();
    bool is_finished();
private:
    PlayerNum player_num;
    FadeAnimation* fade_in;

};
