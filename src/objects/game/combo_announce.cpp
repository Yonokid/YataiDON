#include "combo_announce.h"

ComboAnnounce::ComboAnnounce(int combo, double current_ms, PlayerNum player_num, bool is_2p)
    : combo(combo), wait(current_ms), player_num(player_num), is_2p(is_2p),
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
        audio->play_sound(sound_name, "voice");
        audio_played = true;
    }
}

void ComboAnnounce::draw() {
    if (combo == 0) {
        return;
    }

    float fade_value = is_finished ? fade->attribute : 1 - fade->attribute;

    std::string bg_name = "announce_bg_" + std::to_string(static_cast<int>(player_num)) + "p";
    tex.draw_texture("combo", bg_name, {
        .fade = fade_value,
        .index = (int)is_2p
    });

    if (combo >= 1000) {
        int thousands = combo / 1000;
        int remaining_hundreds = (combo % 1000) / 100;
        float thousands_offset = -110;
        float hundreds_offset = 20;

        if (combo % 1000 == 0) {
            tex.draw_texture("combo", "announce_number", {
                .frame = thousands - 1,
                .x = -23 * tex.screen_scale,
                .fade = fade_value,
                .index = (int)is_2p
            });
            tex.draw_texture("combo", "announce_add", {
                .frame = 0,
                .x = 435 * tex.screen_scale,
                .fade = fade_value,
                .index = (int)is_2p
            });
        } else {
            if (thousands <= 5) {
                tex.draw_texture("combo", "announce_add", {
                    .frame = thousands,
                    .x = 429 * tex.screen_scale + thousands_offset,
                    .fade = fade_value,
                    .index = (int)is_2p
                });
            }
            if (remaining_hundreds > 0) {
                tex.draw_texture("combo", "announce_number", {
                    .frame = remaining_hundreds - 1,
                    .x = hundreds_offset,
                    .fade = fade_value,
                    .index = (int)is_2p
                });
            }
        }
        float text_offset = -30 * tex.screen_scale;
        tex.draw_texture("combo", "announce_text", {
            .x = -text_offset / 2,
            .fade = fade_value,
            .index = (int)is_2p
        });
    } else {
        tex.draw_texture("combo", "announce_number", {
            .frame = combo / 100 - 1,
            .x = 0,
            .fade = fade_value,
            .index = (int)is_2p
        });
        tex.draw_texture("combo", "announce_text", {
            .x = 0,
            .fade = fade_value,
            .index = (int)is_2p
        });
    }
}
