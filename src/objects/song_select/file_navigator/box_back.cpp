#include "box_back.h"

BackBox::BackBox(const fs::path& path, const std::optional<ray::Color> back_color,
        const std::optional<ray::Color> fore_color, TextureIndex texture_index) : BaseBox(path, back_color, fore_color, texture_index) {
}

void BackBox::expand_box() {
    yellow_box.emplace();
    yellow_box_opened = false;
}

void BackBox::draw_closed(float outer_fade_override) {
    BaseBox::draw_closed(outer_fade_override);
    tex.draw_texture("box", "back_text", {.x=position, .fade=outer_fade_override});
}

void BackBox::draw_open(std::optional<float> inner_fade_override) {
    tex.draw_texture("box", "back_text_highlight", {.x=position});
}
