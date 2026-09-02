#include "combo_announce.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"

namespace {

constexpr float CELL       = 104.0f;
constexpr float GROUP_CX   = 204.0f;   // combo_num's origin inside the scroll
constexpr float PITCH_WIDE = 64.0f;
constexpr float PITCH_NARROW = 54.0f;
constexpr float SX_NARROW  = 0.85f;
constexpr float NARROW_X0  = -70.0f;   // leftmost cell, local to GROUP_CX
constexpr float TEXTMC_WIDE   = 392.0f;
constexpr float TEXTMC_NARROW = 398.0f;
constexpr float TEXT_DX = -84.0f;
constexpr float TEXT_W  = 123.0f;

struct Layout {
    float sx;
    float pitch;
    float first_cx;   // centre of the leftmost cell, scroll frame
    float textmc_x;
};

Layout layout(int digits) {
    if (digits <= 3) {
        return {1.0f, PITCH_WIDE,
                GROUP_CX - PITCH_WIDE * (digits - 1) * 0.5f, TEXTMC_WIDE};
    }
    if (digits == 4) {
        return {SX_NARROW, PITCH_NARROW, GROUP_CX + NARROW_X0, TEXTMC_NARROW};
    }
    const float k     = 4.0f / static_cast<float>(digits);
    const float pitch = PITCH_NARROW * k;
    return {SX_NARROW * k, pitch,
            GROUP_CX + 92.0f - pitch * (digits - 1), TEXTMC_NARROW};
}

}  // namespace

ComboAnnounce::ComboAnnounce(int combo, double current_ms, PlayerNum player_num)
    : combo(combo), wait(current_ms), player_num(player_num),
      is_finished(false), audio_played(false) {

    fade = (FadeAnimation*)tex.get_animation(65, true);
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

    if (is_finished && fade->is_finished) {
        return;
    }

    float fade_value = is_finished ? fade->attribute : 1 - fade->attribute;

    const std::string suffix = std::to_string(static_cast<int>(player_num)) + "p";
    tex.draw_texture(tex.get_enum("combo/announce_bg_" + suffix),
                     {.y = y, .fade = fade_value});

    const std::string digit_name = "combo/announce_digit_" + suffix;
    if (tex.has_texture(digit_name)) {
        const std::string number = std::to_string(combo);
        const int n = static_cast<int>(number.size());
        const Layout lay = layout(n);
        const uint32_t digit_id = static_cast<uint32_t>(tex.get_enum(digit_name));
        const float dw = CELL * lay.sx;

        for (int i = 0; i < n; i++) {
            const float cx = lay.first_cx + lay.pitch * i;
            tex.draw_texture(digit_id, {
                .frame = number[i] - '0',
                .x  = cx - dw * 0.5f,
                .y  = y,
                .x2 = dw - CELL,          // horizontal condense only; sy stays 1
                .fade = fade_value,
            });
        }

        tex.draw_texture(COMBO::ANNOUNCE_TEXT, {
            .x  = lay.textmc_x + lay.sx * TEXT_DX,
            .y  = y,
            .x2 = TEXT_W * (lay.sx - 1.0f),
            .fade = fade_value,
        });
        return;
    }

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
