#include "box_yellow.h"
#include "../../../libs/texture.h"

YellowBox::YellowBox()
{
    is_diff_select = false;

    left_out    = (MoveAnimation*)tex.get_animation(9);
    right_out   = (MoveAnimation*)tex.get_animation(10);
    center_out  = (MoveAnimation*)tex.get_animation(11);

    left_out->reset();
    right_out->reset();
    center_out->reset();

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

    TextureObject* bb = tex.textures[YELLOW_BOX::YELLOW_BOX_BOTTOM_RIGHT].get();
    bottom_y    = bb->y[0];
    edge_height = bb->height;
}

void YellowBox::reset() {
    left_out     = (MoveAnimation*)tex.get_animation(9);
    right_out    = (MoveAnimation*)tex.get_animation(10);
    center_out   = (MoveAnimation*)tex.get_animation(11);
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
    if (left_out != nullptr) left_out->update(current_ms);
    if (right_out != nullptr) right_out->update(current_ms);
    if (center_out != nullptr) center_out->update(current_ms);
    if (fade_in != nullptr) fade_in->update(current_ms);
    if (left_out_2 != nullptr) left_out_2->update(current_ms);
    if (right_out_2 != nullptr) right_out_2->update(current_ms);
    if (center_out_2 != nullptr) center_out_2->update(current_ms);
    if (top_y_out != nullptr) top_y_out->update(current_ms);
    if (center_h_out != nullptr) center_h_out->update(current_ms);

    if (is_diff_select) {
        right_x       = right_out_2->attribute;
        left_x        = left_out_2->attribute;
        top_y         = top_y_out->attribute;
        center_width  = center_out_2->attribute;
        center_height = center_h_out->attribute;

        left_distance = left_x - left_out_2->start_position;
        right_distance = right_x - right_out_2->start_position;
    } else {
        right_x       = right_out->attribute;
        left_x        = left_out->attribute;
        center_width  = center_out->attribute;
        top_y         = top_y_out->attribute;
        center_height = center_h_out->attribute;

        left_distance = left_x - left_out->start_position;
        right_distance = right_x + tex.textures[YELLOW_BOX::YELLOW_BOX_RIGHT]->width;
    }
}

void YellowBox::draw(float fade) {
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_BOTTOM_RIGHT, {.x=right_x, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_BOTTOM_LEFT,  {.x=left_x,              .y=bottom_y, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_TOP_RIGHT,    {.x=right_x,             .y=top_y, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_TOP_LEFT,     {.x=left_x,              .y=top_y, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_BOTTOM,       {.x=left_x+edge_height,  .y=bottom_y,           .x2=center_width, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_RIGHT,        {.x=right_x,             .y=top_y+edge_height,  .y2=center_height, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_LEFT,         {.x=left_x,              .y=top_y+edge_height,  .y2=center_height, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_TOP,          {.x=left_x+edge_height,  .y=top_y,              .x2=center_width, .fade=fade});
    tex.draw_texture(YELLOW_BOX::YELLOW_BOX_CENTER,       {.x=left_x+edge_height,  .y=top_y+edge_height,  .x2=center_width, .y2=center_height, .fade=fade});
}
