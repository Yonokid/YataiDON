#include "result.h"

void ResultScreen::on_screen_start() {
    Screen::on_screen_start();
    song_info = new OutlinedText(global_data.session_data[(int)global_data.player_num].song_title, tex.skin_config["song_info_result"].font_size, ray::WHITE, ray::BLACK, false, 5);
    audio->play_sound("bgm", "music");
    fade_out = (FadeAnimation*)tex.get_animation(0);
    fade_in.emplace(global_data.player_num);
    start_ms = get_current_ms();
    background.emplace(global_data.player_num, tex.screen_width);
    player_1.emplace(global_data.player_num, false, false);
    song_num = new SongNum(global_data.songs_played+1);
}

Screens ResultScreen::on_screen_end(Screens next_screen) {
    global_data.songs_played += 1;
    reset_session();
    return Screen::on_screen_end(next_screen);
}

void ResultScreen::handle_input() {
    if (fade_in.has_value() && fade_in->is_finished() && is_r_don_pressed() || is_l_don_pressed()) {
        if (skipped_time == 0) {
            skipped_time = get_current_ms();
            audio->play_sound("don", "sound");
        } else if (get_current_ms() > skipped_time + 5000 && !fade_out->is_started) {
            fade_out->start();
            audio->play_sound("don", "sound");
        }
    }
}

std::optional<Screens> ResultScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    if (fade_in.has_value()) fade_in->update(current_time);
    if (player_1.has_value()) player_1->update(current_time, fade_in.has_value() && fade_in->is_finished(), skipped_time > 0);

    if (current_time >= start_ms + 5000 && fade_out && !fade_out->is_started) {
        handle_input();
    }

    if (fade_out) {
        fade_out->update(current_time);
        if (fade_out->is_finished) {
            fade_out->update(current_time);
            return on_screen_end(Screens::SONG_SELECT);
        }
    }
    return std::nullopt;
}

void ResultScreen::draw_overlay() {
    if (fade_in.has_value()) fade_in->draw();
    if (fade_out) ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Fade(ray::BLACK, fade_out->attribute));
    coin_overlay.draw();
    allnet_indicator.draw();
}

void ResultScreen::draw_song_info() {
    song_num->draw(tex.skin_config["song_num_result"].x, tex.skin_config["song_num_result"].y, 1.0);
    song_info->draw({.x=tex.skin_config["song_info_result"].x - song_info->width, .y=tex.skin_config["song_info_result"].y - song_info->height / 2, .fade=1.0});
}

void ResultScreen::draw() {
    if (background.has_value()) background->draw();
    draw_song_info();
    if (player_1.has_value()) player_1->draw();
    draw_overlay();
}
