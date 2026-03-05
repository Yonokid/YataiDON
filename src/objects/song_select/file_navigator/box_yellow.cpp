#include "box_yellow.h"

YellowBox::YellowBox()
{
    is_diff_select = false;

    left_out    = (MoveAnimation*)tex.get_animation(9);
    right_out   = (MoveAnimation*)tex.get_animation(10);
    center_out  = (MoveAnimation*)tex.get_animation(11);
    fade        = (FadeAnimation*)tex.get_animation(12);

    left_out->reset();
    right_out->reset();
    center_out->reset();
    fade->reset();

    left_out_2   = (MoveAnimation*)tex.get_animation(13);
    right_out_2  = (MoveAnimation*)tex.get_animation(14);
    center_out_2 = (MoveAnimation*)tex.get_animation(15);
    top_y_out    = (MoveAnimation*)tex.get_animation(16);
    center_h_out = (MoveAnimation*)tex.get_animation(17);
    fade_in      = (FadeAnimation*)tex.get_animation(18);

    right_out_2->reset();
    top_y_out->reset();
    center_h_out->reset();

    right_x       = right_out->attribute;
    left_x        = left_out->attribute;
    center_width  = center_out->attribute;
    top_y         = top_y_out->attribute;
    center_height = center_h_out->attribute;

    TextureObject* bb = tex.textures["yellow_box"]["yellow_box_bottom_right"].get();
    bottom_y    = bb->y[0];
    edge_height = bb->height;
}

void YellowBox::reset() {
    left_out     = (MoveAnimation*)tex.get_animation(9);
    right_out    = (MoveAnimation*)tex.get_animation(10);
    center_out   = (MoveAnimation*)tex.get_animation(11);
    fade         = (FadeAnimation*)tex.get_animation(12);
    left_out_2   = (MoveAnimation*)tex.get_animation(13);
    right_out_2  = (MoveAnimation*)tex.get_animation(14);
    center_out_2 = (MoveAnimation*)tex.get_animation(15);
    top_y_out    = (MoveAnimation*)tex.get_animation(16);
    center_h_out = (MoveAnimation*)tex.get_animation(17);
    fade_in      = (FadeAnimation*)tex.get_animation(18);
}

void YellowBox::create_anim() {
    right_out_2->reset();
    top_y_out->reset();
    center_h_out->reset();
    left_out->start();
    right_out->start();
    center_out->start();
    fade->start();
}

void YellowBox::create_anim_2() {
    left_out_2->start();
    right_out_2->start();
    center_out_2->start();
    top_y_out->start();
    center_h_out->start();
    fade_in->start();
    is_diff_select = true;
}

void YellowBox::update(double current_ms) {
    left_out->update(current_ms);
    right_out->update(current_ms);
    center_out->update(current_ms);
    fade->update(current_ms);
    fade_in->update(current_ms);
    left_out_2->update(current_ms);
    right_out_2->update(current_ms);
    center_out_2->update(current_ms);
    top_y_out->update(current_ms);
    center_h_out->update(current_ms);

    if (is_diff_select) {
        right_x       = right_out_2->attribute;
        left_x        = left_out_2->attribute;
        top_y         = top_y_out->attribute;
        center_width  = center_out_2->attribute;
        center_height = center_h_out->attribute;
    } else {
        right_x       = right_out->attribute;
        left_x        = left_out->attribute;
        center_width  = center_out->attribute;
        top_y         = top_y_out->attribute;
        center_height = center_h_out->attribute;
    }
}

void YellowBox::draw_yellow_box() {
    tex.draw_texture("yellow_box", "yellow_box_bottom_right", {.x=right_x});
    tex.draw_texture("yellow_box", "yellow_box_bottom_left",  {.x=left_x,              .y=bottom_y});
    tex.draw_texture("yellow_box", "yellow_box_top_right",    {.x=right_x,             .y=top_y});
    tex.draw_texture("yellow_box", "yellow_box_top_left",     {.x=left_x,              .y=top_y});
    tex.draw_texture("yellow_box", "yellow_box_bottom",       {.x=left_x+edge_height,  .y=bottom_y,           .x2=center_width});
    tex.draw_texture("yellow_box", "yellow_box_right",        {.x=right_x,             .y=top_y+edge_height,  .y2=center_height});
    tex.draw_texture("yellow_box", "yellow_box_left",         {.x=left_x,              .y=top_y+edge_height,  .y2=center_height});
    tex.draw_texture("yellow_box", "yellow_box_top",          {.x=left_x+edge_height,  .y=top_y,              .x2=center_width});
    tex.draw_texture("yellow_box", "yellow_box_center",       {.x=left_x+edge_height,  .y=top_y+edge_height,  .x2=center_width, .y2=center_height});
}

void YellowBox::draw() {
    if (fade->attribute > 0.0f) draw_yellow_box();
}
