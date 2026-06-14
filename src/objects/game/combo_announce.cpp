#include "combo_announce.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"

ComboAnnounce::ComboAnnounce(int combo, double current_ms, PlayerNum player_num)
    : combo(combo), wait(current_ms), player_num(player_num),
      is_finished(false), audio_played(false) {

    fade = (FadeAnimation*)tex.get_animation(65);
    fade->start();
}

void ComboAnnounce::update(double current_ms) {
    if (current_ms >= wait + 1666.67f && !is_finished) {
        fade->start();
        is_finished = true;
    }

    fade->update(current_ms);

    if (!audio_played && combo >= 100) {
        std::string sound_name = "combo_" + std::to_string(combo) + "_" + std::to_string(static_cast<int>(player_num)) + "p";
        audio.play_sound(sound_name, VolumePreset::VOICE);
        audio_played = true;
    }
}

void ComboAnnounce::draw(float y) {
    if (combo == 0) {
        return;
    }

    float fade_value = is_finished ? fade->attribute : 1 - fade->attribute;

    std::string bg_name = "announce_bg_" + std::to_string(static_cast<int>(player_num)) + "p";
    tex.draw_texture(tex.get_enum("combo/" + (bg_name)), {.y=y, .fade = fade_value,});

    if (combo >= 1000) {
        int thousands = combo / 1000;
        int remaining_hundreds = (combo % 1000) / 100;
        float thousands_offset = tex.skin_config[SC::COMBO_ANNOUNCE_THOUSANDS_OFFSET].x;
        float hundreds_offset = tex.skin_config[SC::COMBO_ANNOUNCE_HUNDREDS_OFFSET].x;

        if (combo % 1000 == 0) {
            tex.draw_texture(COMBO::ANNOUNCE_NUMBER, {.frame = thousands - 1, .x = tex.skin_config[SC::COMBO_ANNOUNCE_NUMBER_THOUSANDS_X].x, .y = y, .fade = fade_value});
            tex.draw_texture(COMBO::ANNOUNCE_ADD, {.frame = 0, .x = tex.skin_config[SC::COMBO_ANNOUNCE_ADD_X].x, .y = y, .fade = fade_value});
        } else {
            if (thousands <= 5) {
                tex.draw_texture(COMBO::ANNOUNCE_ADD, {.frame = thousands, .x = tex.skin_config[SC::COMBO_ANNOUNCE_THOUSANDS_ADD_X].x + thousands_offset, .y = y, .fade = fade_value});
            }
            if (remaining_hundreds > 0) {
                tex.draw_texture(COMBO::ANNOUNCE_NUMBER, {.frame = remaining_hundreds - 1, .x = hundreds_offset, .y = y, .fade = fade_value});
            }
        }
        float text_offset = tex.skin_config[SC::COMBO_ANNOUNCE_TEXT_OFFSET].x;
        tex.draw_texture(COMBO::ANNOUNCE_TEXT, {.x = -text_offset / 2, .y = y, .fade = fade_value});
    } else {
        tex.draw_texture(COMBO::ANNOUNCE_NUMBER, {.frame = combo / 100 - 1, .x = 0, .y = y, .fade = fade_value});
        tex.draw_texture(COMBO::ANNOUNCE_TEXT, {.x = 0, .y = y, .fade = fade_value});
    }
}
