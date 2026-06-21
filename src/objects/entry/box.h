#pragma once

#include "../../libs/script.h"
#include "../../libs/screen.h"

class Box : public LuaScript {
private:
    float x;
    float y;
    float static_x;
    float left_x;
    float static_left;
    float right_x;
    float static_right;
    float y_pos;
    float static_y;
    MoveAnimation* open;
    bool is_selected;
    bool moving_left;
    bool moving_right;
    bool moving_up;
    bool moving_down;

    sol::protected_function fn_draw;

public:
    float width;
    Screens location;
    MoveAnimation* move;
    Box(const std::string& text_str, int font_size, Screens location);
    void set_positions(float x, float y = 0);
    void update(double current_time_ms, bool is_selected);
    void move_left();
    void move_right();
    void move_up();
    void move_down();
    void draw(float fade);
};
