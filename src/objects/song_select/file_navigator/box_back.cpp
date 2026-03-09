#include "box_back.h"

BackBox::BackBox(const fs::path& path, const std::optional<ray::Color> back_color,
        const std::optional<ray::Color> fore_color, TextureIndex texture_index) : BaseBox(path, back_color, fore_color, texture_index) {
}

void BackBox::draw_closed() {
    BaseBox::draw_closed();
    tex.draw_texture("box", "back_text", {.x=position});
}

void BackBox::draw_open() {
    if (yellow_box.has_value())
        yellow_box->draw();
    float x = position + (yellow_box->right_out->attribute*0.85 - (yellow_box->right_out->start_position*0.85)) + yellow_box->right_out_2->attribute - yellow_box->right_out_2->start_position;
    tex.draw_texture("box", "back_text_highlight", {.x=x});
    tex.draw_texture("box", "back_graphic", {.fade=open_fade->attribute});
}
