#include "input_cali.h"
#include "../libs/input.h"

static const fs::path CALI_TJA_PATH = "Songs/Calibration.tja";

void InputCaliScreen::on_screen_start() {
    SessionData& session_data = global_data.session_data[(int)global_data.player_num];
    session_data.selected_song = CALI_TJA_PATH;
    global_data.modifiers[(int)global_data.player_num].auto_play = true;
    session_data.selected_difficulty = 2;
    GameScreen::on_screen_start();

    background.reset();
    background.emplace(global_data.player_num, 150, "TUTORIAL");
    average_latency = 0.0;
}

Screens InputCaliScreen::on_screen_end(Screens next_screen) {
    global_data.modifiers[(int)global_data.player_num].auto_play = false;
    global_data.config->general.audio_offset = average_latency * -1;
    return GameScreen::on_screen_end(next_screen);
}

std::optional<Screens> InputCaliScreen::update() {
    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::SETTINGS);
    }

    auto result = GameScreen::update();
    if (result == Screens::SONG_SELECT) {
        return std::nullopt;
    }
    else if (result == Screens::RESULT) {
        return Screens::SETTINGS;
    }

    if (is_l_don_pressed() || is_r_don_pressed()) {
        double current_ms = get_current_ms();
        double latency = players[0]->last_note_hit - current_ms;
        latencies.push_back(latency);
        size_t n = latencies.size();
        std::sort(latencies.begin(), latencies.end());

        if (n % 2 == 0) {
            average_latency = (latencies[n/2 - 1] + latencies[n/2]) / 2.0;
        } else {
            average_latency = latencies[n/2];
        }
        average_latency_text.reset();
        average_latency_text.emplace(ray::TextFormat("Average Latency: %.2f ms", average_latency * -1), 40, ray::WHITE, ray::BLUE, false, 4.0);
    }

    return result;
}

void InputCaliScreen::draw() {
    ray::ClearBackground(ray::BLACK);
    if (background.has_value()) background->draw_back();
    players[0]->draw(current_ms, 0, 232 * tex.screen_scale, mask_shader);
    if (average_latency_text.has_value()) {
        average_latency_text->draw({.x=(int)(tex.screen_width/2) - average_latency_text->width/2, .y=static_cast<float>(tex.screen_height - 40 - (int)(average_latency_text->height*1.5))});
    }
    if (background.has_value()) background->draw_fore();
    draw_overlay();
}
