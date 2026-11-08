#include "retry_game.h"

#include "../libs/file/scores.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"
#include "total_result.h"

void RetryGameScreen::on_screen_start() {
    Screen::on_screen_start();
    auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num));
    scene.emplace(static_cast<int>(global_data.player_num), needed_hits);

    chara = make_chara_from_player_data(pd ? &*pd : nullptr);
    if (pd) {
        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara->apply_face(pd->chara_face_index);
    } else {
        chara->set_don_colors(chara_default_color_1(global_data.player_num),
                              chara_default_color_2(global_data.player_num), {249, 240, 225, 255});
    }
    chara->set_rotation(tex.chara_3d_config.rot_x, 0.0f, tex.chara_3d_config.rot_z);
    chara->set_anim(AnimIndex::DON_FUKKATU_LOOP);
    chara->set_frozen(true);
    chara_phase = "wait";
    chara_target = 0.0f;
    chara_progress = 0.0f;
    last_ms = 0;
}

Screens RetryGameScreen::on_screen_end(Screens next_screen) {
    scene.reset();
    chara.reset();
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> RetryGameScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    if (!scene.has_value()) {
        return std::nullopt;
    }
    double current_ms = get_current_ms();
    if (last_ms == 0) {
        last_ms = current_ms;
    }
    allnet_indicator.update(current_ms);

    int hits = is_l_don_pressed() + is_r_don_pressed();
    for (int i = 0; i < hits; i++) {
        scene->hit();
        audio.play_sound("don", VolumePreset::SOUND);
        if (scene->phase() == "count") {
            chara_target += 0.5f;
        }
    }
    scene->update(current_ms);
    if (chara) {
        std::string phase = scene->phase();
        if (phase != chara_phase) {
            chara_phase = phase;
            if (phase == "success") {
                chara->set_anim(AnimIndex::DON_FULL_GAGE);
            } else if (phase == "failure") {
                chara->set_anim(AnimIndex::DON_MISS_NORMAL);
            }
            if (phase == "success" || phase == "failure") {
                chara->set_frozen(false);
            }
        }
        if (phase == "count" && chara_progress < chara_target) {
            chara_progress =
                std::min(chara_target, chara_progress + 0.5f * static_cast<float>(current_ms - last_ms) / 100.0f);
            chara->set_anim_progress(chara_progress);
        }
        chara->update(current_ms);
    }
    last_ms = current_ms;

    if (scene->is_finished()) {
        const auto& network_config = global_data.config->network;
        const std::string& access_code =
            global_data.player_num == PlayerNum::P2 ? network_config.access_code_2 : network_config.access_code_1;
        if (!access_code.empty()) {
            network.submit_revive_result(access_code, scene->is_success());
        }
        if (scene->is_success()) {
            return on_screen_end(select_screen());
        }
        reset_session();
        if (global_data.config->limits.song_limit > 0 &&
            global_data.config->limits.song_limit == global_data.songs_played) {
            return end_or_game_over(begin_total_result());
        }
        global_data.returned_from_result = true;
        return on_screen_end(select_screen());
    }
    return std::nullopt;
}

void RetryGameScreen::draw() {
    if (!scene.has_value()) {
        return;
    }
    scene->draw_back();
    if (chara) {
        auto pos = scene->chara_pos();
        if (pos) {
            chara->draw((*pos)[0], (*pos)[1], (*pos)[2]);
        }
    }
    scene->draw_front();
    coin_overlay.draw();
    allnet_indicator.draw();
}
