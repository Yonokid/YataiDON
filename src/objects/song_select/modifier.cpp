#include "modifier.h"
#include "../../libs/audio.h"

const std::map<std::string, std::string> ModifierSelector::TEX_MAP = {
    {"auto",    "mod_auto"},
    {"speed",   "mod_baisaku"},
    {"display", "mod_doron"},
    {"inverse", "mod_abekobe"},
    {"random",  "mod_kimagure"}
};
const std::array<std::string, 5> ModifierSelector::MOD_NAMES = {
    "auto", "speed", "display", "inverse", "random"
};

// Maps mod_index to the bool fields in Modifiers (auto, display, inverse)
bool ModifierSelector::get_bool(int mod_index) {
    switch (mod_index) {
        case 0: return player->modifier_auto;
        case 2: return player->modifier_display;
        case 3: return player->modifier_inverse;
        default: return false;
    }
}

void ModifierSelector::set_bool(int mod_index, bool value) {
    switch (mod_index) {
        case 0: player->modifier_auto    = value; break;
        case 2: player->modifier_display = value; break;
        case 3: player->modifier_inverse = value; break;
        default: break;
    }
}

std::unique_ptr<OutlinedText> ModifierSelector::make_text(const std::string& str) {
    return std::make_unique<OutlinedText>(str, tex.skin_config[SC::MODIFIER_TEXT].font_size, ray::WHITE, ray::BLACK, false, 3.5f);
}

ModifierSelector::ModifierSelector(PlayerNum player_num, PlayerData* player) : player_num(player_num), player(player) {
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

    audio.play_sound("voice_options_" + std::to_string((int)player_num) + "p", VolumePreset::SOUND);

    static const std::array<SC, 5> MOD_NAME_KEYS = {
        SC::MODIFIER_NAME_AUTO, SC::MODIFIER_NAME_SPEED, SC::MODIFIER_NAME_DISPLAY,
        SC::MODIFIER_NAME_INVERSE, SC::MODIFIER_NAME_RANDOM
    };
    for (const auto& key : MOD_NAME_KEYS)
        text_name.push_back(make_text(tex.skin_config[key].text.at(language)));

    text_true      = make_text(tex.skin_config[SC::MODIFIER_TEXT_TRUE].text.at(language));
    text_false     = make_text(tex.skin_config[SC::MODIFIER_TEXT_FALSE].text.at(language));
    text_speed     = make_text(std::format("{:.1f}", player->modifier_speed / 10.0f));
    text_kimagure  = make_text(tex.skin_config[SC::MODIFIER_TEXT_KIMAGURE].text.at(language));
    text_detarame  = make_text(tex.skin_config[SC::MODIFIER_TEXT_DETARAME].text.at(language));

    text_true_2     = make_text(tex.skin_config[SC::MODIFIER_TEXT_TRUE].text.at(language));
    text_false_2    = make_text(tex.skin_config[SC::MODIFIER_TEXT_FALSE].text.at(language));
    text_speed_2    = make_text(std::format("{:.1f}", player->modifier_speed / 10.0f));
    text_kimagure_2 = make_text(tex.skin_config[SC::MODIFIER_TEXT_KIMAGURE].text.at(language));
    text_detarame_2 = make_text(tex.skin_config[SC::MODIFIER_TEXT_DETARAME].text.at(language));
}

void ModifierSelector::update(double current_ms) {
    move->update(current_ms);
    blue_arrow_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    move_sideways->update(current_ms);
    fade_sideways->update(current_ms);
    is_finished = is_confirmed && move->is_finished;
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
    if (mod_name == "speed") {
        text_speed_2 = std::move(text_speed);
        text_speed = make_text(std::format("{:.1f}", player->modifier_speed / 10.0f));
    } else if (mod_name == "random") {
        if (player->modifier_random == 1) {
            text_kimagure = std::move(text_kimagure_2);
            text_kimagure_2 = make_text(tex.skin_config[SC::MODIFIER_TEXT_KIMAGURE].text.at(language));
        } else if (player->modifier_random == 2) {
            text_detarame = std::move(text_detarame_2);
            text_detarame_2 = make_text(tex.skin_config[SC::MODIFIER_TEXT_DETARAME].text.at(language));
        }
    } else {
        // bool mod
        if (get_bool(current_mod_index)) {
            text_true = std::move(text_true_2);
            text_true_2 = make_text(tex.skin_config[SC::MODIFIER_TEXT_TRUE].text.at(language));
        } else {
            text_false = std::move(text_false_2);
            text_false_2 = make_text(tex.skin_config[SC::MODIFIER_TEXT_FALSE].text.at(language));
        }
    }
}

void ModifierSelector::left() {
    if (is_confirmed) return;
    const std::string& mod_name = MOD_NAMES[current_mod_index];

    if (mod_name == "speed") {
        player->modifier_speed = std::max(1, player->modifier_speed - 1);
        start_text_animation(-1);
    } else if (mod_name == "random") {
        player->modifier_random = std::max(0, player->modifier_random - 1);
        start_text_animation(-1);
    } else {
        set_bool(current_mod_index, !get_bool(current_mod_index));
        start_text_animation(-1);
    }
}

void ModifierSelector::right() {
    if (is_confirmed) return;
    const std::string& mod_name = MOD_NAMES[current_mod_index];

    if (mod_name == "speed") {
        player->modifier_speed += 1;
        start_text_animation(1);
    } else if (mod_name == "random") {
        player->modifier_random = (player->modifier_random + 1) % 3;
        start_text_animation(1);
    } else {
        set_bool(current_mod_index, !get_bool(current_mod_index));
        start_text_animation(1);
    }
}

void ModifierSelector::draw_animated_text(const std::unique_ptr<OutlinedText>& primary, const std::unique_ptr<OutlinedText>& secondary, float x, float y, bool should_animate) {
    if (should_animate && !move_sideways->is_finished) {
        primary->draw({
            .x = x + ((float)move_sideways->attribute * direction),
            .y = y,
            .fade = fade_sideways->attribute});
        secondary->draw({
            .x = (direction * -tex.skin_config[SC::OPTION_TEXT_IN].x) + x + ((float)move_sideways->attribute * direction),
            .y = y,
            .fade = 1.0f - fade_sideways->attribute});
    } else {
        primary->draw({.x = x, .y = y});
    }
}

void ModifierSelector::draw() {
    float move_val = is_confirmed
        ? move->attribute + tex.skin_config[SC::SONG_SELECT_OFFSET].x
        : -move->attribute;
    float x = ((int)player_num - 1) * tex.skin_config[SC::OPTION_P2].x;
    float mod_offset_y = tex.skin_config[SC::MODIFIER_OFFSET].y;

    tex.draw_texture(MODIFIER::TOP,    {.x=x, .y=move_val});
    tex.draw_texture(tex.get_enum("modifier/" + (std::to_string((int)player_num) + "p")), {.x=x, .y=move_val});
    tex.draw_texture(MODIFIER::BOTTOM, {.x=x, .y=move_val + ((int)MOD_NAMES.size() * mod_offset_y)});

    for (int i = 0; i < (int)MOD_NAMES.size(); i++) {
        float row_y = move_val + (i * mod_offset_y);
        const std::string& mod_name = MOD_NAMES[i];
        bool is_current = (i == current_mod_index);

        tex.draw_texture(MODIFIER::BACKGROUND,                              {.x=x, .y=row_y});
        tex.draw_texture(tex.get_enum(std::string("modifier/") + (is_current ? "mod_bg_highlight" : "mod_bg")), {.x=x, .y=row_y});
        tex.draw_texture(MODIFIER::MOD_BOX,                                 {.x=x, .y=row_y});

        text_name[i]->draw({
            .x = tex.skin_config[SC::MODIFIER_OFFSET_2].x + x,
            .y = tex.skin_config[SC::MODIFIER_TEXT].y + row_y
        });

        float text_base_x = tex.skin_config[SC::MODIFIER_TEXT].x;
        float text_y = tex.skin_config[SC::MODIFIER_TEXT].y + row_y;

        if (mod_name == "speed") {
            float tx = text_base_x - (text_speed->width / 2.0f);
            draw_animated_text(text_speed, text_speed_2, tx + x, text_y, is_current);

            float spd = player->modifier_speed;
            if      (spd >= 40) tex.draw_texture(MODIFIER::MOD_YONBAI,         {.x=x, .y=row_y});
            else if (spd >= 30) tex.draw_texture(MODIFIER::MOD_SANBAI,         {.x=x, .y=row_y});
            else if (spd >  10) tex.draw_texture(tex.get_enum("modifier/" + (TEX_MAP.at(mod_name))), {.x=x, .y=row_y});

        } else if (mod_name == "random") {
            if (player->modifier_random == 1) {
                float tx = text_base_x - (text_kimagure->width / 2.0f);
                draw_animated_text(text_kimagure, text_kimagure_2, tx + x, text_y, is_current);
                tex.draw_texture(tex.get_enum("modifier/" + (TEX_MAP.at(mod_name))), {.x=x, .y=row_y});
            } else if (player->modifier_random == 2) {
                float tx = text_base_x - (text_detarame->width / 2.0f);
                draw_animated_text(text_detarame, text_detarame_2, tx + x, text_y, is_current);
                tex.draw_texture(MODIFIER::MOD_DETARAME, {.x=x, .y=row_y});
            } else {
                float tx = text_base_x - (text_false->width / 2.0f);
                draw_animated_text(text_false, text_false_2, tx + x, text_y, is_current);
            }

        } else {
            // bool mod
            bool val = get_bool(i);
            if (val) tex.draw_texture(tex.get_enum("modifier/" + (TEX_MAP.at(mod_name))), {.x=x, .y=row_y});
            const auto& primary   = val ? text_true   : text_false;
            const auto& secondary = val ? text_true_2 : text_false_2;
            float tx = text_base_x - (primary->width / 2.0f);
            draw_animated_text(primary, secondary, tx + x, text_y, is_current);
        }

        if (is_current) {
            tex.draw_texture(MODIFIER::BLUE_ARROW, {.x=x - (float)blue_arrow_move->attribute, .y=row_y, .fade=blue_arrow_fade->attribute});
            tex.draw_texture(MODIFIER::BLUE_ARROW, {.mirror=Mirror::HORIZONTAL, .x=x + tex.skin_config[SC::MODIFIER_OFFSET_2].y + (float)blue_arrow_move->attribute, .y=row_y, .fade=blue_arrow_fade->attribute});
        }
    }
}
