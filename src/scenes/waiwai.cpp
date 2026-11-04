#include "waiwai.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include "../libs/file/scores.h"
#include "../libs/input/input.h"

static const std::string WAIWAI_BEGIN_VOICE_1P = "voice_waiwai_begin_1p";
static const std::string WAIWAI_BEGIN_VOICE_2P = "voice_waiwai_begin_2p";

void WaiwaiSongSelectScreen::on_screen_start() {
    SongSelect2PScreen::on_screen_start();
    player->shared_start_voice = WAIWAI_BEGIN_VOICE_1P;
    player_2->shared_start_voice = WAIWAI_BEGIN_VOICE_2P;
    overlay.emplace("WaiwaiSongSelect", "waiwai_song_select");
    don_hits = 0;
}

Screens WaiwaiSongSelectScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    return SongSelect2PScreen::on_screen_end(next_screen);
}

std::optional<Screens> WaiwaiSongSelectScreen::update() {
    auto next = SongSelect2PScreen::update();
    if (overlay) {
        overlay->update(get_current_ms(), don_hits);
    }
    return next;
}

void WaiwaiSongSelectScreen::handle_input(double current_ms) {
    if (overlay && overlay->loaded() && !overlay->is_finished()) {
        while (is_l_don_pressed() || is_r_don_pressed()) {
            don_hits++;
        }
        clear_input_buffers();
        return;
    }
    SongSelect2PScreen::handle_input(current_ms);
}

void WaiwaiSongSelectScreen::select_song(SongBox* song) {
    audio.play_sound(WAIWAI_BEGIN_VOICE_1P, VolumePreset::VOICE);
    audio.play_sound(WAIWAI_BEGIN_VOICE_2P, VolumePreset::VOICE);
    SongSelect2PScreen::select_song(song);
}

void WaiwaiSongSelectScreen::draw() {
    SongSelect2PScreen::draw();
    if (overlay) {
        overlay->draw(get_current_ms());
    }
}

void WaiwaiTutorialScreen::on_screen_start() {
    Screen::on_screen_start();
    overlay.emplace("WaiwaiTutorial", "waiwai_tutorial");
    start_ms = get_current_ms();
    kat_hits = 0;
}

Screens WaiwaiTutorialScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> WaiwaiTutorialScreen::update() {
    if (auto init = Screen::update()) {
        return init;
    }
    while (is_l_kat_pressed() || is_r_kat_pressed()) {
        kat_hits++;
    }
    overlay->update(get_current_ms() - start_ms, kat_hits);
    if (!overlay->loaded() || overlay->is_finished()) {
        return on_screen_end(Screens::WAIWAI_SONG_SELECT);
    }
    return std::nullopt;
}

void WaiwaiTutorialScreen::draw() {
    clear_background(Color::BLACK);
    overlay->draw(get_current_ms() - start_ms);
}

namespace {
    // no synchro tracking yet, so the mean of both players' gauges stands in for the shared one
    int waiwai_gauge_point(float mean_fraction, bool cleared) {
        const int point = static_cast<int>(std::lround(mean_fraction * 50.0f));
        return cleared ? std::clamp(point, 35, 50) : std::clamp(point, 0, 34);
    }

    constexpr float kStageCharaX[2] = {510.0f, 770.0f};
    constexpr float kStageCharaY = 710.0f;

    Player::SynchroArt load_synchro_art(const std::string& stem) {
        auto part = [&](std::string_view name) { return tex.get_texture(std::format("effect/{}_{}", stem, name)); };
        return {.glow = part("glow"),
                .arm = {part("arm"), part("arm_miss_0"), part("arm_miss_1")},
                .face = {part("face_0"), part("face_1")}};
    }
} // namespace

void WaiwaiGameScreen::apply_shared_art() {
    if (!overlay || !overlay->loaded() || !tex.has_texture("effect/gauge_back")) {
        return;
    }
    for (auto& player : players) {
        const std::string side = player->player_num == PlayerNum::P2 ? "2p" : "1p";
        player->hide_gauge = true;
        player->rainbow_arc = true;
        player->place_chara_on_stage(kStageCharaX[player->player_num == PlayerNum::P2] * tex.screen_scale,
                                     kStageCharaY * tex.screen_scale);
        player->synchro_art = {load_synchro_art("synchro_big_don_" + side),
                               load_synchro_art("synchro_big_katsu_" + side)};
    }
}

void WaiwaiGameScreen::initialize_chart(const fs::path& song) {
    Game2PScreen::initialize_chart(song);
    apply_shared_art();
}

void WaiwaiGameScreen::on_screen_start() {
    Game2PScreen::on_screen_start();
    overlay.emplace("WaiwaiGame", "waiwai_game");
    apply_shared_art();
}

Screens WaiwaiGameScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    return Game2PScreen::on_screen_end(next_screen);
}

std::optional<Screens> WaiwaiGameScreen::update() {
    auto next = Game2PScreen::update();
    if (overlay && !players.empty()) {
        const double end_time = std::max(players.front()->end_time, players.back()->end_time);
        overlay->update(clock.position() - start_delay, clock.position() >= end_time ? 1 : 0);
        const Player& p1 = *players.front();
        const Player& p2 = *players.back();
        if (p1.gauge && p2.gauge) {
            overlay->set_gauge(waiwai_gauge_point((p1.gauge->get_length() + p2.gauge->get_length()) / 200.0f,
                                                  p1.gauge->get_is_clear() && p2.gauge->get_is_clear()));
        }
    }
    return next;
}

void WaiwaiGameScreen::draw_players() {
    Game2PScreen::draw_players();
    for (auto& player : players) {
        player->draw_stage_chara();
    }
    if (overlay) {
        overlay->draw(clock.position() - start_delay);
    }
    for (auto& player : players) {
        player->draw_arcs();
    }
}

namespace {
    constexpr float kResultCharaX[2] = {190.0f, 1100.0f};
    constexpr float kResultCharaY = 470.0f;

    struct WaiwaiScore {
        int gauge_point; // 0-50, 35 and up is a clear, 50 is amazing
        int duet_percent;
        int message; // 0 very bad, 1 bad, 2 good, 3 excellent
    };

    // no synchro tracking yet, so both players' accuracy stands in for the duet rate
    WaiwaiScore waiwai_score() {
        float gauge = 0.0f;
        int judged = 0;
        int weighted = 0;
        bool cleared = true;
        for (PlayerNum pn : {PlayerNum::P1, PlayerNum::P2}) {
            const ResultData& rd = global_data.session_data[static_cast<int>(pn)].result_data;
            gauge += rd.gauge_length / 87.0f / 2.0f;
            judged += rd.good + rd.ok + rd.bad;
            weighted += rd.good * 2 + rd.ok;
            cleared = cleared && rd.cleared;
        }
        const int point = waiwai_gauge_point(gauge, cleared);
        const int percent = judged > 0 ? weighted * 100 / (judged * 2) : 0;
        const int message = point >= 50 ? 3 : (point >= 35 ? 2 : (point >= 17 ? 1 : 0));
        return {point, percent, message};
    }
} // namespace

void WaiwaiResultScreen::on_screen_start() {
    ResultScreen::on_screen_start();
    player_1.reset();
    background.reset();
    if (loading_graphic.has_value()) {
        unload_texture(loading_graphic.value());
        loading_graphic.reset();
    }
    audio.stop_sound("bgm");
    audio.load_screen_sounds("waiwai_result");
    fade_in.emplace(PlayerNum::TWO_PLAYER);
    reveal_done = false;
    chara_motion = -1;
    for (PlayerNum pn : {PlayerNum::P1, PlayerNum::P2}) {
        const bool is_2p = pn == PlayerNum::P2;
        auto pd = scores_manager.get_player_data(get_player_id(pn));
        auto chara = make_chara_from_player_data(pd ? &*pd : nullptr, is_2p);
        if (pd) {
            chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
            chara->apply_face(pd->chara_face_index);
        } else {
            chara->set_don_colors(chara_default_color_1(pn), chara_default_color_2(pn), {249, 240, 225, 255});
        }
        chara->set_anim(AnimIndex::DON_NORMAL);
        charas[is_2p] = std::move(chara);
    }
    overlay.emplace("WaiwaiResult", "waiwai_result");
    const WaiwaiScore score = waiwai_score();
    overlay->set_result(score.gauge_point, score.duet_percent, score.message, retry_pending());
}

Screens WaiwaiResultScreen::on_screen_end(Screens next_screen) {
    overlay.reset();
    charas = {};
    return ResultScreen::on_screen_end(next_screen);
}

bool WaiwaiResultScreen::run_failed() const {
    for (PlayerNum pn : {PlayerNum::P1, PlayerNum::P2}) {
        if (!global_data.session_data[static_cast<int>(pn)].result_data.cleared) {
            return true;
        }
    }
    return false;
}

double WaiwaiResultScreen::reveal_end_ms() {
    return reveal_done ? get_current_ms() : 0.0;
}

std::optional<Screens> WaiwaiResultScreen::update() {
    Screen::update();
    const double current_ms = get_current_ms();
    fade_in->update(current_ms);
    for (auto& chara : charas) {
        chara->update(current_ms);
    }
    update_input_and_timeout(current_ms);
    if (overlay && fade_in->is_finished()) {
        overlay->update(current_ms, skipped_time > 0 ? 1 : 0);
        reveal_done = !overlay->loaded() || overlay->is_finished();
        const int motion = overlay->motion();
        if (motion != chara_motion && motion >= 0) {
            chara_motion = motion;
            const AnimIndex anim = motion == 0   ? AnimIndex::DON_MISS_NORMAL
                                   : motion == 1 ? AnimIndex::DON_NORMAL
                                                 : AnimIndex::DON_SABI;
            for (auto& chara : charas) {
                chara->set_anim(anim);
            }
        }
        if (reveal_done && !exiting()) {
            begin_exit();
        }
    }
    if (retrying) {
        if (fade_in->is_finished()) {
            if (!retry_message) {
                retry_message.emplace();
            }
            retry_message->update(current_ms);
            if (retry_message->is_finished()) {
                return on_screen_end(Screens::WAIWAI_RETRY_GAME);
            }
        }
    } else if (fade_out) {
        fade_out->update(current_ms);
        if (fade_out->is_finished) {
            if (quitting || (global_data.config->limits.song_limit > 0 &&
                             global_data.config->limits.song_limit == global_data.songs_played)) {
                return on_screen_end(credit_end_screen());
            }
            global_data.returned_from_result = true;
            return on_screen_end(Screens::WAIWAI_SONG_SELECT);
        }
    }
    return std::nullopt;
}

void WaiwaiResultScreen::draw() {
    const double current_ms = get_current_ms();
    if (overlay) {
        overlay->draw(current_ms);
    }
    for (int i = 0; i < 2; i++) {
        charas[i]->draw(kResultCharaX[i] * tex.screen_scale, kResultCharaY * tex.screen_scale);
    }
    if (overlay) {
        overlay->draw_front(current_ms);
    }
    draw_overlay();
}
