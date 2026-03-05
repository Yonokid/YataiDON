#include "modifier.h"

const std::map<std::string, std::string> ModifierSelector::TEX_MAP = {
    {"auto",    "mod_auto"},
    {"speed",   "mod_baisaku"},
    {"display", "mod_doron"},
    {"inverse", "mod_abekobe"},
    {"random",  "mod_kimagure"}
};
const std::map<std::string, std::string> ModifierSelector::NAME_MAP_JA = {
    {"auto",    "オート"},
    {"speed",   "はやさ"},
    {"display", "ドロン"},
    {"inverse", "あべこべ"},
    {"random",  "ランダム"}
};
const std::map<std::string, std::string> ModifierSelector::NAME_MAP_EN = {
    {"auto",    "Auto"},
    {"speed",   "Speed"},
    {"display", "Display"},
    {"inverse", "Inverse"},
    {"random",  "Random"}
};
const std::array<std::string, 5> ModifierSelector::MOD_NAMES = {
    "auto", "speed", "display", "inverse", "random"
};

// Maps mod_index to the bool fields in Modifiers (auto, display, inverse)
bool ModifierSelector::get_bool(int mod_index) {
    auto& m = global_data.modifiers[(int)player_num];
    switch (mod_index) {
        case 0: return m.auto_play;
        case 2: return m.display;
        case 3: return m.inverse;
        default: return false;
    }
}

void ModifierSelector::set_bool(int mod_index, bool value) {
    auto& m = global_data.modifiers[(int)player_num];
    switch (mod_index) {
        case 0: m.auto_play = value; break;
        case 2: m.display   = value; break;
        case 3: m.inverse   = value; break;
        default: break;
    }
}

OutlinedText* ModifierSelector::make_text(const std::string& str) {
    return new OutlinedText(str, tex.skin_config["modifier_text"].font_size, ray::WHITE, ray::BLACK, false, 3.5f);
}

ModifierSelector::ModifierSelector(PlayerNum player_num) : player_num(player_num) {
    current_mod_index = 0;
    is_confirmed = false;
    is_finished = false;
    direction = -1;
    language = global_data.config->general.language;

    blue_arrow_fade = (FadeAnimation*)tex.get_animation(29, true);
    blue_arrow_move = (MoveAnimation*)tex.get_animation(30, true);
    move            = (MoveAnimation*)tex.get_animation(28, true);
    move->start();
    move_sideways   = (MoveAnimation*)tex.get_animation(31, true);
    fade_sideways   = (FadeAnimation*)tex.get_animation(32, true);

    audio->play_sound("voice_options_" + std::to_string((int)player_num) + "p", "sound");

    const auto& name_map = (language == "en") ? NAME_MAP_EN : NAME_MAP_JA;
    for (const auto& name : MOD_NAMES)
        text_name.push_back(make_text(name_map.at(name)));

    text_true      = make_text(tex.skin_config["modifier_text_true"].text.at(language));
    text_false     = make_text(tex.skin_config["modifier_text_false"].text.at(language));
    text_speed     = make_text(std::to_string(global_data.modifiers[(int)player_num].speed));
    text_kimagure  = make_text(tex.skin_config["modifier_text_kimagure"].text.at(language));
    text_detarame  = make_text(tex.skin_config["modifier_text_detarame"].text.at(language));

    text_true_2     = make_text(tex.skin_config["modifier_text_true"].text.at(language));
    text_false_2    = make_text(tex.skin_config["modifier_text_false"].text.at(language));
    text_speed_2    = make_text(std::to_string(global_data.modifiers[(int)player_num].speed));
    text_kimagure_2 = make_text(tex.skin_config["modifier_text_kimagure"].text.at(language));
    text_detarame_2 = make_text(tex.skin_config["modifier_text_detarame"].text.at(language));
}

void ModifierSelector::update(double current_ms) {
    is_finished = is_confirmed && move->is_finished;
    move->update(current_ms);
    blue_arrow_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    move_sideways->update(current_ms);
    fade_sideways->update(current_ms);
}

void ModifierSelector::confirm() {
    if (is_confirmed) return;
    current_mod_index++;
    if (current_mod_index == (int)MOD_NAMES.size()) {
        is_confirmed = true;
        move->restart();
    }
}

void ModifierSelector::start_text_animation(int dir) {
    move_sideways->start();
    fade_sideways->start();
    direction = dir;

    const std::string& mod_name = MOD_NAMES[current_mod_index];
    auto& modifiers = global_data.modifiers[(int)player_num];

    if (mod_name == "speed") {
        delete text_speed_2;
        text_speed_2 = text_speed;
        text_speed = make_text(std::to_string(modifiers.speed));
    } else if (mod_name == "random") {
        if (modifiers.random == 1) {
            delete text_kimagure;
            text_kimagure = text_kimagure_2;
            text_kimagure_2 = make_text(tex.skin_config["modifier_text_kimagure"].text.at(language));
        } else if (modifiers.random == 2) {
            delete text_detarame;
            text_detarame = text_detarame_2;
            text_detarame_2 = make_text(tex.skin_config["modifier_text_detarame"].text.at(language));
        }
    } else {
        // bool mod
        if (get_bool(current_mod_index)) {
            delete text_true;
            text_true = text_true_2;
            text_true_2 = make_text(tex.skin_config["modifier_text_true"].text.at(language));
        } else {
            delete text_false;
            text_false = text_false_2;
            text_false_2 = make_text(tex.skin_config["modifier_text_false"].text.at(language));
        }
    }
}

void ModifierSelector::left() {
    if (is_confirmed) return;
    const std::string& mod_name = MOD_NAMES[current_mod_index];
    auto& modifiers = global_data.modifiers[(int)player_num];

    if (mod_name == "speed") {
        modifiers.speed = std::max(0.1f, (float)((int)(modifiers.speed * 10) - 1) / 10.0f);
        start_text_animation(-1);
    } else if (mod_name == "random") {
        modifiers.random = std::max(0, modifiers.random - 1);
        start_text_animation(-1);
    } else {
        set_bool(current_mod_index, !get_bool(current_mod_index));
        start_text_animation(-1);
    }
}

void ModifierSelector::right() {
    if (is_confirmed) return;
    const std::string& mod_name = MOD_NAMES[current_mod_index];
    auto& modifiers = global_data.modifiers[(int)player_num];

    if (mod_name == "speed") {
        modifiers.speed = (float)((int)(modifiers.speed * 10) + 1) / 10.0f;
        start_text_animation(1);
    } else if (mod_name == "random") {
        modifiers.random = (modifiers.random + 1) % 3;
        start_text_animation(1);
    } else {
        set_bool(current_mod_index, !get_bool(current_mod_index));
        start_text_animation(1);
    }
}

void ModifierSelector::draw_animated_text(OutlinedText* primary, OutlinedText* secondary, float x, float y, bool should_animate) {
    if (should_animate && !move_sideways->is_finished) {
        primary->draw({
            .x = x + ((float)move_sideways->attribute * direction),
            .y = y,
            .fade = fade_sideways->attribute});
        secondary->draw({
            .x = (direction * -tex.skin_config["option_text_in"].x) + x + ((float)move_sideways->attribute * direction),
            .y = y,
            .fade = 1.0f - fade_sideways->attribute});
    } else {
        primary->draw({.x = x, .y = y});
    }
}

void ModifierSelector::draw() {
    float move_val = is_confirmed
        ? move->attribute - tex.skin_config["song_select_offset"].x
        : -move->attribute;
    float x = ((int)player_num - 1) * tex.skin_config["option_p2"].x;
    float mod_offset_y = tex.skin_config["modifier_offset"].y;

    tex.draw_texture("modifier", "top",    {.x=x, .y=move_val});
    tex.draw_texture("modifier", std::to_string((int)player_num) + "p", {.x=x, .y=move_val});
    tex.draw_texture("modifier", "bottom", {.x=x, .y=move_val + ((int)MOD_NAMES.size() * mod_offset_y)});

    for (int i = 0; i < (int)MOD_NAMES.size(); i++) {
        float row_y = move_val + (i * mod_offset_y);
        const std::string& mod_name = MOD_NAMES[i];
        auto& modifiers = global_data.modifiers[(int)player_num];
        bool is_current = (i == current_mod_index);

        tex.draw_texture("modifier", "background",                              {.x=x, .y=row_y});
        tex.draw_texture("modifier", is_current ? "mod_bg_highlight" : "mod_bg", {.x=x, .y=row_y});
        tex.draw_texture("modifier", "mod_box",                                 {.x=x, .y=row_y});

        text_name[i]->draw({
            .x = tex.skin_config["modifier_offset_2"].x + x,
            .y = tex.skin_config["modifier_text"].y + row_y
        });

        float text_base_x = tex.skin_config["modifier_text"].x;
        float text_y = tex.skin_config["modifier_text"].y + row_y;

        if (mod_name == "speed") {
            float tx = text_base_x - (text_speed->width / 2.0f);
            draw_animated_text(text_speed, text_speed_2, tx + x, text_y, is_current);

            float spd = modifiers.speed;
            if      (spd >= 4.0f) tex.draw_texture("modifier", "mod_yonbai",         {.x=x, .y=row_y});
            else if (spd >= 3.0f) tex.draw_texture("modifier", "mod_sanbai",         {.x=x, .y=row_y});
            else if (spd >  1.0f) tex.draw_texture("modifier", TEX_MAP.at(mod_name), {.x=x, .y=row_y});

        } else if (mod_name == "random") {
            if (modifiers.random == 1) {
                float tx = text_base_x - (text_kimagure->width / 2.0f);
                draw_animated_text(text_kimagure, text_kimagure_2, tx + x, text_y, is_current);
                tex.draw_texture("modifier", TEX_MAP.at(mod_name), {.x=x, .y=row_y});
            } else if (modifiers.random == 2) {
                float tx = text_base_x - (text_detarame->width / 2.0f);
                draw_animated_text(text_detarame, text_detarame_2, tx + x, text_y, is_current);
                tex.draw_texture("modifier", "mod_detarame", {.x=x, .y=row_y});
            } else {
                float tx = text_base_x - (text_false->width / 2.0f);
                draw_animated_text(text_false, text_false_2, tx + x, text_y, is_current);
            }

        } else {
            // bool mod
            bool val = get_bool(i);
            if (val) tex.draw_texture("modifier", TEX_MAP.at(mod_name), {.x=x, .y=row_y});
            OutlinedText* primary   = val ? text_true   : text_false;
            OutlinedText* secondary = val ? text_true_2 : text_false_2;
            float tx = text_base_x - (primary->width / 2.0f);
            draw_animated_text(primary, secondary, tx + x, text_y, is_current);
        }

        if (is_current) {
            tex.draw_texture("modifier", "blue_arrow", {.x=x - (float)blue_arrow_move->attribute, .y=row_y, .fade=blue_arrow_fade->attribute});
            tex.draw_texture("modifier", "blue_arrow", {.mirror="horizontal", .x=x + tex.skin_config["modifier_offset_2"].y + (float)blue_arrow_move->attribute, .y=row_y, .fade=blue_arrow_fade->attribute});
        }
    }
}
