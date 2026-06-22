#include "box_back.h"

BackBox::BackBox(const fs::path& path, const BoxDef& box_def) : BaseBox(path, box_def) {
    this->text_name = "BACK_BOX";
}

void BackBox::draw_closed() {
    BaseBox::draw_closed();
    tex.draw_texture(BOX::BACK_TEXT, {.x=box_x(), .y=box_y(), .fade=fade->attribute});
}

void BackBox::draw_open() {
    float bx = box_x();
    float by = box_y();
    float mfade = std::min(fade->attribute, open_fade->attribute);
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_LEFT,  {.x=bx, .y=by, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM,       {.x=bx, .y=by, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_RIGHT, {.x=bx, .y=by, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_RIGHT,        {.x=bx, .y=by, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_TOP_RIGHT,    {.x=bx, .y=by, .fade=mfade, .index=1});
    if (yellow_box.has_value())
        yellow_box->draw(mfade, by);
    float x = bx + (yellow_box->right_out->attribute*0.85 - (yellow_box->right_out->start_position*0.85)) + yellow_box->right_out_2->attribute - yellow_box->right_out_2->start_position;
    tex.draw_texture(BOX::BACK_TEXT_HIGHLIGHT, {.x=x, .y=by, .fade=mfade});
    tex.draw_texture(BOX::BACK_GRAPHIC, {.y=by, .fade=mfade});
}
