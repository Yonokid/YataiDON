#include "box.h"

#include <stdexcept>

static constexpr int ENTRY_BOX_MOVE_ANIM_ID = 10;
static constexpr int ENTRY_BOX_OPEN_ANIM_ID = 11;

static const char* mode_key(Screens location) {
    switch (location) {
    case Screens::SONG_SELECT:
    case Screens::SONG_SELECT_2P:
        return "entry_game";
    case Screens::DAN_SELECT:
        return "entry_dan";
    case Screens::AI_SELECT:
        return "entry_ai_battle";
    case Screens::PRACTICE_SELECT:
        return "entry_practice";
    case Screens::ENTRY:
        return "entry_costume";
    case Screens::REWARD_SHOP:
        return "entry_shop";
    case Screens::WAIWAI_SONG_SELECT:
        return "entry_waiwai";
    case Screens::SETTINGS:
        return "entry_settings";
    default:
        return "";
    }
}

Box::Box(const std::string& text_str, int font_size, Screens location, const std::string& mode_key_override)
    : location(location) {
    auto box_tex = tex.textures.find("mode_select/box");
    if (box_tex == tex.textures.end() || !box_tex->second) {
        spdlog::error("Box: texture 'mode_select/box' is missing");
        throw std::runtime_error("Box: missing texture");
    }
    x = box_tex->second->x[0];
    y = box_tex->second->y[0];
    width = box_tex->second->width;
    move = dynamic_cast<MoveAnimation*>(tex.get_animation(ENTRY_BOX_MOVE_ANIM_ID));
    open = dynamic_cast<MoveAnimation*>(tex.get_animation(ENTRY_BOX_OPEN_ANIM_ID));
    if (move == nullptr || open == nullptr) {
        spdlog::error("Box: animation {} or {} is missing/not a MoveAnimation", ENTRY_BOX_MOVE_ANIM_ID,
                      ENTRY_BOX_OPEN_ANIM_ID);
        throw std::runtime_error("Box: required MoveAnimation missing");
    }
    is_selected = false;
    moving_left = false;
    moving_right = false;
    moving_up = false;
    moving_down = false;
    y_pos = 0;
    static_y = 0;
    static_x = x;
    left_x = x;
    static_left = left_x;
    right_x = left_x + tex.textures["mode_select/box"]->width - tex.textures["mode_select/box_highlight_right"]->width;
    static_right = right_x;
    if (!load("EntryBox", "box", text_str, font_size,
              mode_key_override.empty() ? std::string(mode_key(location)) : mode_key_override)) {
        spdlog::error("Box: failed to load EntryBox/box script; draw() will be a no-op");
        return;
    }
    fn_draw = lua_object["draw"];
}

void Box::set_positions(float x, float y) {
    this->x = x;
    static_x = this->x;
    left_x = this->x;
    static_left = left_x;
    right_x = left_x + tex.textures["mode_select/box"]->width - tex.textures["mode_select/box_highlight_right"]->width;
    static_right = right_x;
    this->y_pos = y;
    this->static_y = y;
    moving_left = moving_right = moving_up = moving_down = false;
    move->reset();
}

void Box::update(double current_ms, bool is_selected) {
    move->update(current_ms);
    if (moving_left) {
        x = static_x - move->attribute;
    } else if (moving_right) {
        x = static_x + move->attribute;
    } else if (moving_up) {
        y_pos = static_y - move->attribute;
    } else if (moving_down) {
        y_pos = static_y + move->attribute;
    }
    if (move->is_finished) {
        moving_left = false;
        moving_right = false;
        moving_up = false;
        moving_down = false;
        static_x = x;
        static_y = y_pos;
    }
    if (is_selected && !this->is_selected) {
        open->start();
    }
    this->is_selected = is_selected;
    open->update(current_ms);
    if (is_selected) {
        left_x = static_left - open->attribute;
        right_x = static_right + open->attribute;
    } else {
        left_x = static_left;
        right_x = static_right;
    }
}

void Box::move_left() {
    if (!move->is_started || move->is_finished) {
        move->start();
    }
    moving_right = moving_up = moving_down = false;
    moving_left = true;
}

void Box::move_right() {
    if (!move->is_started || move->is_finished) {
        move->start();
    }
    moving_left = moving_up = moving_down = false;
    moving_right = true;
}

void Box::move_up() {
    if (!move->is_started || move->is_finished) {
        move->start();
    }
    moving_left = moving_right = moving_down = false;
    moving_up = true;
}

void Box::move_down() {
    if (!move->is_started || move->is_finished) {
        move->start();
    }
    moving_left = moving_right = moving_up = false;
    moving_down = true;
}

void Box::draw(float fade, bool has_prev, bool has_next, bool disabled) {
    call(fn_draw, "EntryBox:draw", x, left_x, right_x, is_selected, move->is_finished,
         static_cast<float>(open->attribute), fade, y_pos, has_prev, has_next, disabled);
}
