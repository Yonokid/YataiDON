#pragma once

#include "../../../libs/animation.h"

class YellowBox {
public:
    bool is_diff_select;

    MoveAnimation* left_out;
    MoveAnimation* right_out;
    MoveAnimation* center_out;

    MoveAnimation* left_out_2;
    MoveAnimation* right_out_2;
    MoveAnimation* center_out_2;
    MoveAnimation* top_y_out;
    MoveAnimation* center_h_out;
    FadeAnimation* fade_in;

    float right_x;
    float left_x;
    float center_width;
    float top_y;
    float center_height;
    float bottom_y;
    float edge_height;

    float left_distance;
    float right_distance;

    YellowBox();

    void reset();
    void create_anim();
    void create_anim_2();
    void update(double current_ms);
    void draw(float fade = 1.0);

private:
};
