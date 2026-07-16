#include "title.h"
#include "../libs/global_data.h"
#include "../libs/input.h"
#include <random>

void TitleScreen::on_screen_start() {
    Screen::on_screen_start();
    load_videos();
    state = TitleState::OP_VIDEO;
    hit_taiko_text = std::make_unique<OutlinedText>(tex.skin_config[SC::HIT_TAIKO_TO_START].text[global_data.config->general.language], tex.skin_config[SC::HIT_TAIKO_TO_START].font_size, ray::WHITE, ray::BLACK, false, 4);
    fade_out = (FadeAnimation*)tex.get_animation(13);
    text_overlay_fade = (FadeAnimation*)tex.get_animation(14);
}

void TitleScreen::load_videos() {
    op_video_list.clear();
    attract_video_list.clear();
    fs::path videos_path = fs::path("Skins" / global_data.config->paths.skin / "Videos");
    if (!fs::exists(videos_path)) {
        spdlog::error("Error: videos folder not found");
        return;
    }
    fs::path op_path = videos_path / "op_videos";
    if (fs::exists(op_path)) {
        for (const auto& entry : fs::recursive_directory_iterator(op_path)) {
            if (entry.path().extension() == ".mp4")
                op_video_list.push_back(entry.path());
        }
    } else {
        spdlog::warn("op_videos folder not found");
    }

    fs::path attract_path = videos_path / "attract_videos";
    if (fs::exists(attract_path)) {
        for (const auto& entry : fs::recursive_directory_iterator(attract_path)) {
            if (entry.path().extension() == ".mp4")
                attract_video_list.push_back(entry.path());
        }
    } else {
        spdlog::warn("attract_videos folder not found");
    }
}

Screens TitleScreen::on_screen_end(Screens next_screen) {
    op_video.reset();
    attract_video.reset();
    return Screen::on_screen_end(next_screen);
}

void TitleScreen::scene_manager(double current_ms) {
    if (state == TitleState::OP_VIDEO) {
        if (!op_video.has_value()) {
            if (op_video_list.empty()) { state = TitleState::WARNING; return; }
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
            if (attract_video_list.empty()) { state = TitleState::OP_VIDEO; return; }
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
            state = TitleState::ATTRACT_CAMERA;
        }
    } else if (state == TitleState::ATTRACT_CAMERA) {
        if (!attract_camera.has_value()) {
            attract_camera.emplace();
            bana_advert_1.emplace();
            bana_advert_2.emplace();
            camera_cloud.emplace();
        }
        attract_camera->update(current_ms);
        bana_advert_1->update(current_ms);
        bana_advert_2->update(current_ms);
        camera_cloud->update(current_ms);
        if (attract_camera->is_finished()) {
            attract_camera.reset();
            state = TitleState::OP_VIDEO;
            bana_advert_1.reset();
            bana_advert_2.reset();
            camera_cloud.reset();
        }
    }
}

std::optional<Screens> TitleScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();

    text_overlay_fade->update(current_ms);
    fade_out->update(current_ms);
    allnet_indicator.update(current_ms);
    entry_overlay.update(current_ms);

    if (fade_out->is_finished) {
        return on_screen_end(Screens::ENTRY);
    }

    scene_manager(current_ms);
    if (is_l_don_pressed() || is_r_don_pressed()) {
        fade_out->start();
        audio.play_sound("don", VolumePreset::SOUND);
    }
    return std::nullopt;
}

void TitleScreen::draw() {
    if (state == TitleState::OP_VIDEO && op_video) {
        op_video->draw();
    } else if (state == TitleState::WARNING && warning_board) {
        tex.draw_texture(WARNING::BACKGROUND);
        warning_board->draw();
    } else if (state == TitleState::ATTRACT_VIDEO && attract_video) {
        attract_video->draw();
    } else if (state == TitleState::ATTRACT_CAMERA && attract_camera) {
        attract_camera->draw();
        camera_cloud->draw();
        bana_advert_1->draw(33, 136);
        bana_advert_2->draw(1023, 136);
    }

    tex.draw_texture(MOVIE::BACKGROUND, {.fade=fade_out->attribute});
    coin_overlay.draw();
    allnet_indicator.draw();
    entry_overlay.draw(tex.skin_config[SC::ENTRY_OVERLAY_TITLE].x, tex.skin_config[SC::ENTRY_OVERLAY_TITLE].y);

    hit_taiko_text->draw({.x=(float)(tex.screen_width*0.25 - hit_taiko_text->width/2), .y=tex.skin_config[SC::HIT_TAIKO_TO_START].y, .fade=text_overlay_fade->attribute});
    hit_taiko_text->draw({.x=(float)(tex.screen_width*0.75 - hit_taiko_text->width/2), .y=tex.skin_config[SC::HIT_TAIKO_TO_START].y, .fade=text_overlay_fade->attribute});
}
