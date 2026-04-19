#include "box.h"

Box::Box(std::unique_ptr<OutlinedText> text, Screens location) : text(std::move(text)), location(location) {
    x = tex.textures[MODE_SELECT::BOX]->x[0];
    y = tex.textures[MODE_SELECT::BOX]->y[0];
    width = tex.textures[MODE_SELECT::BOX]->width;
    move = (MoveAnimation*)tex.get_animation(10);
    open = (MoveAnimation*)tex.get_animation(11);
    is_selected = false;
    moving_left = false;
    moving_right = false;
}

void Box::set_positions(float x) {
    this->x = x;
    static_x = this->x;
    left_x = this->x;
    static_left = left_x;
    right_x = left_x + tex.textures[MODE_SELECT::BOX]->width - tex.textures[MODE_SELECT::BOX_HIGHLIGHT_RIGHT]->width;
    static_right = right_x;
}

void Box::update(double current_ms, bool is_selected) {
    move->update(current_ms);
    if (moving_left) {
        x = static_x - move->attribute;
    } else if (moving_right) {
        x = static_x + move->attribute;
    }
    if (move->is_finished) {
        moving_left = false;
        moving_right = false;
        static_x = x;
    }
    if (is_selected && !this->is_selected) {
        open->start();
    }
    this->is_selected = is_selected;
    if (is_selected) {
        left_x = static_left - open->attribute;
        right_x = static_right + open->attribute;
    }
    open->update(current_ms);
}

void Box::move_left() {
    if (!move->is_started) {
        move->start();
    }
    moving_left = true;
}

void Box::move_right() {
    if (!move->is_started) {
        move->start();
    }
    moving_right = true;
}

void Box::draw_highlighted(float fade) {
    tex.draw_texture(MODE_SELECT::BOX_HIGHLIGHT_CENTER, {.x=left_x + tex.textures[MODE_SELECT::BOX_HIGHLIGHT_LEFT]->width, .y=y, .x2=right_x - left_x + tex.skin_config[SC::ENTRY_BOX_HIGHLIGHT_OFFSET].x, .fade=fade});
    tex.draw_texture(MODE_SELECT::BOX_HIGHLIGHT_LEFT, {.x=left_x, .y=y, .fade=fade});
    tex.draw_texture(MODE_SELECT::BOX_HIGHLIGHT_RIGHT, {.x=right_x, .y=y, .fade=fade});
}

void Box::draw_text(float fade) {
    float text_x = x + ((float)tex.textures[MODE_SELECT::BOX]->width / 2) - (text->width / 2);
    if (is_selected) {
        text_x += open->attribute;
    }
    float text_y = y + tex.skin_config[SC::ENTRY_BOX_TEXT_OFFSET].y;
    text->draw({.x=text_x, .y=text_y, .fade=fade});
}

void Box::draw(float fade) {
    if (is_selected && move->is_finished) {
        draw_highlighted(fade);
    } else {
        tex.draw_texture(MODE_SELECT::BOX, {.x=x, .fade=fade});
    }
    draw_text(fade);
}
