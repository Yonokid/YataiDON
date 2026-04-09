#include "title.h"

void TitleScreen::on_screen_start() {
    Screen::on_screen_start();
    op_video_list.clear();
    attract_video_list.clear();
    fs::path videos_path = fs::path("Skins" / global_data.config->paths.skin / "Videos");
    for (const auto& entry : fs::recursive_directory_iterator(videos_path / "op_videos")) {
        if (entry.path().extension() == ".mp4") {
            op_video_list.push_back(entry.path());
        }
    }

    for (const auto& entry : fs::recursive_directory_iterator(videos_path / "attract_videos")) {
        if (entry.path().extension() == ".mp4") {
            attract_video_list.push_back(entry.path());
        }
    }
    state = TitleState::OP_VIDEO;
    hit_taiko_text = new OutlinedText(tex.skin_config["hit_taiko_to_start"].text[global_data.config->general.language], tex.skin_config["hit_taiko_to_start"].font_size, ray::WHITE, ray::BLACK, false, 4);
    fade_out = (FadeAnimation*)tex.get_animation(13);
    text_overlay_fade = (FadeAnimation*)tex.get_animation(14);
}

Screens TitleScreen::on_screen_end(Screens next_screen) {
    // Destroy the VideoPlayer objects (their destructors call stop()).
    // Resetting the optionals ensures scene_manager creates a fresh
    // VideoPlayer on the next visit rather than updating a stopped one.
    op_video.reset();
    attract_video.reset();
    return Screen::on_screen_end(next_screen);
}

void TitleScreen::scene_manager(double current_ms) {
    if (state == TitleState::OP_VIDEO) {
        if (!op_video.has_value()) {
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, op_video_list.size() - 1);
            fs::path chosen = op_video_list[dist(rng)];
            op_video.emplace(chosen);
            op_video->start(current_ms);
        }
        op_video->update(current_ms);
        if (op_video->is_finished()) {
            op_video->stop();
            op_video.reset();
            state = TitleState::WARNING;
        }
    } else if (state == TitleState::WARNING) {
        if (!warning_board.has_value()) {
            warning_board.emplace(current_ms);
        }
        warning_board->update(current_ms);
        if (warning_board->is_finished()) {
            warning_board.reset();
            state = TitleState::ATTRACT_VIDEO;
        }
    } else if (state == TitleState::ATTRACT_VIDEO) {
        if (!attract_video.has_value()) {
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, attract_video_list.size() - 1);
            fs::path chosen = attract_video_list[dist(rng)];
            attract_video.emplace(chosen);
            attract_video->start(current_ms);
        }
        attract_video->update(current_ms);
        if (attract_video->is_finished()) {
            attract_video->stop();
            attract_video.reset();
            state = TitleState::OP_VIDEO;
        }
    }
}

std::optional<Screens> TitleScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();

    text_overlay_fade->update(current_ms);
    fade_out->update(current_ms);

    if (fade_out->is_finished) {
        return on_screen_end(Screens::ENTRY);
    }

    scene_manager(current_ms);
    if (is_l_don_pressed() || is_r_don_pressed()) {
        fade_out->start();
        audio->play_sound("don", "sound");
    }
    return std::nullopt;
}

void TitleScreen::draw() {
    if (state == TitleState::OP_VIDEO && op_video) {
        op_video->draw();
    } else if (state == TitleState::WARNING && warning_board) {
        tex.draw_texture("warning", "background");
        warning_board->draw();
    } else if (state == TitleState::ATTRACT_VIDEO && attract_video) {
        attract_video->draw();
    }

    tex.draw_texture("movie", "background", {.fade=fade_out->attribute});
    coin_overlay.draw();
    allnet_indicator.draw();
    entry_overlay.draw(tex.skin_config["entry_overlay_title"].x, tex.skin_config["entry_overlay_title"].y);

    hit_taiko_text->draw({.x=(float)(tex.screen_width*0.25 - hit_taiko_text->width/2), .y=tex.skin_config["hit_taiko_to_start"].y, .fade=text_overlay_fade->attribute});
    hit_taiko_text->draw({.x=(float)(tex.screen_width*0.75 - hit_taiko_text->width/2), .y=tex.skin_config["hit_taiko_to_start"].y, .fade=text_overlay_fade->attribute});
}
