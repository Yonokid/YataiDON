#include "game_over.h"
#include "../../libs/audio.h"
#include "../../libs/global_data.h"

void GameOverScreen::on_screen_start() {
    Screen::on_screen_start();
    ad_played = false;
    voice_played = false;
    curtain_pull_out = (MoveAnimation*)tex.get_animation(1);
    curtain_pull_in = (MoveAnimation*)tex.get_animation(2);
    kitsune_texture_change = (TextureChangeAnimation*)tex.get_animation(3);
    kitsune_texture_change->start();
    text_bounce_down = (MoveAnimation*)tex.get_animation(4);
    text_bounce_up = (MoveAnimation*)tex.get_animation(5);
    text_bounce_down_2 = (MoveAnimation*)tex.get_animation(6);
    fade_out = (FadeAnimation*)tex.get_animation(7);
    has_copyright  = tex.has_animation(8) && tex.has_animation(9)
                     && tex.has_texture("global/copyright");
    copyright_fade = has_copyright ? (FadeAnimation*)tex.get_animation(8) : nullptr;
    copyright_hold = has_copyright ? (FadeAnimation*)tex.get_animation(9) : nullptr;
    blackout_cue   = tex.has_animation(10) ? (FadeAnimation*)tex.get_animation(10) : nullptr;
    audio.play_sound("bana_ad", VolumePreset::MUSIC);
}

Screens GameOverScreen::on_screen_end(Screens next_screen) {
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> GameOverScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();
    allnet_indicator.update(current_ms);
    if (!audio.is_sound_playing("bana_ad") && !ad_played) {
        ad_played = true;
        audio.play_sound("curtain", VolumePreset::SOUND);
        curtain_pull_out->start();
        curtain_pull_in->start();
        text_bounce_down->start();
        text_bounce_up->start();
        text_bounce_down_2->start();
        audio.play_sound("jingle", VolumePreset::MUSIC);
        if (blackout_cue) blackout_cue->start();
    }
    if (text_bounce_down->attribute > 0 && !voice_played) {
        voice_played = true;
        audio.play_sound("voice", VolumePreset::VOICE);
    }
    if (blackout_cue) {
        blackout_cue->update(current_ms);
        if (blackout_cue->is_finished && !fade_out->is_started) fade_out->start();
    } else if (!audio.is_sound_playing("jingle") && voice_played && !fade_out->is_started) {
        fade_out->start();
    }
    curtain_pull_out->update(current_ms);
    curtain_pull_in->update(current_ms);
    kitsune_texture_change->update(current_ms);
    text_bounce_down->update(current_ms);
    text_bounce_up->update(current_ms);
    text_bounce_down_2->update(current_ms);
    fade_out->update(current_ms);

    if (fade_out->is_finished) {
        if (!has_copyright) return on_screen_end(Screens::TITLE);
        if (!copyright_fade->is_started) copyright_fade->start();
        copyright_fade->update(current_ms);
        if (copyright_fade->is_finished) {
            if (!copyright_hold->is_started) copyright_hold->start();
            copyright_hold->update(current_ms);
            if (copyright_hold->is_finished) return on_screen_end(Screens::TITLE);
        }
    }

    return std::nullopt;
}

void GameOverScreen::draw() {
    tex.draw_texture(LOGGED_OUT::BANA_AD);
    tex.draw_texture(GLOBAL::KITSUNE, {.frame=(int)kitsune_texture_change->attribute, .x=(float)curtain_pull_out->attribute});
    tex.draw_texture(GLOBAL::CURTAIN_SIDE, {.x=(float)curtain_pull_out->attribute + (float)curtain_pull_in->attribute});
    for (int i = 0; i < 4; i++) {
        tex.draw_texture(GLOBAL::CURTAIN, {.x=(i * tex.textures[GLOBAL::CURTAIN]->width) + (float)curtain_pull_out->attribute + (float)curtain_pull_in->attribute});
    }
    std::string text_key = "global/game_over_text_" + global_data.config->general.language;
    if (!tex.has_texture(text_key)) text_key = "global/game_over_text_en";
    tex.draw_texture(tex.get_enum(text_key), {.y=(float)text_bounce_down->attribute + (float)text_bounce_up->attribute + (float)text_bounce_down_2->attribute});
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Color(33, 29, 30, fade_out->attribute * 255));
    if (has_copyright)
        tex.draw_texture(tex.get_enum("global/copyright"), {.fade=(float)copyright_fade->attribute});
    coin_overlay.draw();
    allnet_indicator.draw();
}
