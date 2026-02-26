#include "loading.h"
#include "raylib.h"

void LoadingScreen::on_screen_start() {
    progress_bar_width = tex.screen_width * 0.43;
    progress_bar_height = 50 * tex.screen_scale;
    progress_bar_x = (tex.screen_width - progress_bar_width) / 2;
    progress_bar_y = tex.screen_height * 0.85;

    fade_in = new FadeAnimation(tex.screen_width, tex.screen_height, 0.0, 1.0, 1.0);
    allnet_indicator = AllNetIcon();

    loading_thread = std::thread(&LoadingScreen::load_song_hashes, this);
}

void LoadingScreen::load_song_hashes() {
    //global_data.song_hashes = build_song_hashes();
    songs_loaded = true;
}

void LoadingScreen::load_navigator() {
    //navigator.initialize(global_data.config->paths.tja_path]);
    loading_complete = true;
}

Screens LoadingScreen::on_screen_end(Screens next_screen) {
    if (loading_thread.joinable()) {
        loading_thread.join();
    }
    if (navigator_thread.joinable()) {
        navigator_thread.join();
    }
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> LoadingScreen::update() {
    Screen::update();
    if (songs_loaded && !navigator_started) {
        navigator_thread = std::thread(&LoadingScreen::load_navigator, this);
        navigator_started = true;
    }

    if (loading_complete && !fade_in->isStarted()) {
        fade_in->start();
    }

    fade_in->update(get_current_ms());
    if (fade_in->is_finished) {
        return on_screen_end(Screens::TITLE);
    }

    return std::nullopt;
}

void LoadingScreen::draw() {
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::BLACK);
    tex.draw_texture("kidou", "warning");

    ray::DrawRectangle(progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, ray::Color(101, 0, 0, 255));

    float progress = std::max(0.0f, std::min(1.0f, global_data.song_progress));
    float fill_width = progress_bar_width * progress;
    if (fill_width > 0) {
        ray::DrawRectangle(progress_bar_x, progress_bar_y, fill_width, progress_bar_height, ray::RED);
    }

    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Fade(ray::WHITE, fade_in->attribute));
    allnet_indicator.draw();
}
