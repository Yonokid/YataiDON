#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/text.h"
#include "../../libs/screen.h"

class Box {
private:
    OutlinedText* text;
    float x;
    float y;
    float static_x;
    float left_x;
    float static_left;
    float right_x;
    float static_right;
    MoveAnimation* open;
    bool is_selected;
    bool moving_left;
    bool moving_right;

    void draw_highlighted(float fade);

    void draw_text(float fade);

public:
    float width;
    Screens location;
    MoveAnimation* move;
    Box(OutlinedText* text, Screens location);
    void set_positions(float x);
    void update(double current_time_ms, bool is_selected);
    void move_left();
    void move_right();
    void draw(float fade);
};
