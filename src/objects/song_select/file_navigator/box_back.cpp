#include "box_back.h"

BackBox::BackBox(const fs::path& path, const BoxDef& box_def) : BaseBox(path, box_def) {
    this->text_name = "BACK_BOX";
}

void BackBox::draw_closed() {
    BaseBox::draw_closed();
    tex.draw_texture(BOX::BACK_TEXT, {.x=position, .fade=fade->attribute});
}

void BackBox::draw_open() {
    float mfade = std::min(fade->attribute, open_fade->attribute);
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_LEFT, {.x=position, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM, {.x=position, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_RIGHT, {.x=position, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_RIGHT, {.x=position, .fade=mfade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_TOP_RIGHT, {.x=position, .fade=mfade, .index=1});
    if (yellow_box.has_value())
        yellow_box->draw(mfade);
    float x = position + (yellow_box->right_out->attribute*0.85 - (yellow_box->right_out->start_position*0.85)) + yellow_box->right_out_2->attribute - yellow_box->right_out_2->start_position;
    tex.draw_texture(BOX::BACK_TEXT_HIGHLIGHT, {.x=x, .fade=mfade});
    tex.draw_texture(BOX::BACK_GRAPHIC, {.fade=mfade});
}
