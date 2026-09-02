#include "result_2p.h"
#include <algorithm>

void Result2PScreen::on_screen_start() {
    ResultScreen::on_screen_start();
    if (loading_graphic.has_value()) {
        ray::UnloadTexture(loading_graphic.value());
        loading_graphic.reset();
    }
    background.emplace(PlayerNum::TWO_PLAYER, tex.screen_width);
    fade_in.emplace(PlayerNum::TWO_PLAYER);
    player_1.emplace(PlayerNum::P1, true, false);
    player_2.emplace(PlayerNum::P2, true, true);
}

double Result2PScreen::reveal_end_ms() {
    double a = player_1.has_value() ? player_1->reveal_end_ms() : 0.0;
    double b = player_2.has_value() ? player_2->reveal_end_ms() : 0.0;
    if (a == 0 || b == 0) return 0.0;
    return std::max(a, b);
}

std::optional<Screens> Result2PScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    if (fade_in.has_value()) fade_in->update(current_time);
    if (player_1.has_value()) player_1->update(current_time, fade_in.has_value() && fade_in->is_finished(), skipped_time > 0);
    if (player_2.has_value()) player_2->update(current_time, fade_in.has_value() && fade_in->is_finished(), skipped_time > 0);

    update_input_and_timeout(current_time);

    if (fade_out) {
        fade_out->update(current_time);
        if (fade_out->is_finished) {
            fade_out->update(current_time);
            global_data.returned_from_result = true;
            return on_screen_end(Screens::SONG_SELECT_2P);
        }
    }
    return std::nullopt;
}

void Result2PScreen::draw() {
    if (background.has_value()) background->draw();
    draw_song_info();
    if (player_1.has_value()) player_1->draw();
    if (player_2.has_value()) player_2->draw();
    draw_overlay();
}
