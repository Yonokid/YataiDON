#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/ray.h"
#include "../enums.h"

class ScoreCounterAnimation {
private:
    bool is_2p;
    int counter;
    int direction;
    FadeAnimation* fade_animation_1;
    MoveAnimation* move_animation_1;
    FadeAnimation* fade_animation_2;
    MoveAnimation* move_animation_2;
    MoveAnimation* move_animation_3;
    MoveAnimation* move_animation_4;
    ray::Color base_color;
    ray::Color color;
    std::string counter_str;
    float total_width;
    float margin;
    std::vector<float> y_pos_list;

public:
    ScoreCounterAnimation(PlayerNum player_num, int counter, bool is_2p);

    void update(double current_ms);
    void draw();

    bool is_finished() const;
};
