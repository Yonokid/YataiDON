#include "total_result.h"

#include "../libs/file/scores.h"
#include "../libs/file/script.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"

static constexpr double kAnswerTimeoutMs = 5000.0;

static Screens after_total_result();

static void clear_credit_tally() {
    for (int i = 0; i < 2; i++) {
        global_data.credit_gauges[i].clear();
        global_data.credit_titles[i].clear();
    }
}

Screens begin_total_result() {
    global_data.songs_played = 0;
    network.reset_don_points();
    const std::string* codes[2] = {&global_data.config->network.access_code_1,
                                   &global_data.config->network.access_code_2};
    for (int seat = 0; seat < 2; seat++) {
        global_data.credit_seats[seat] = !global_data.credit_gauges[seat].empty();
        if (!codes[seat]->empty()) {
            network.submit_don_points(seat, *codes[seat], global_data.credit_gauges[seat]);
        }
    }
    if (!tex.options[SCO::DISABLE_TOTAL_RESULT] && script_manager.has_lua_script("total_result")) {
        return Screens::TOTAL_RESULT;
    }
    clear_credit_tally(); // the screen does this on exit; left in place, the next credit would pay these songs again
    return after_total_result();
}

static Screens after_total_result() {
    if (!tex.options[SCO::DISABLE_REWARD_SHOP] && script_manager.has_lua_script("reward_shop") &&
        (network.medal_seat(0).answered || network.medal_seat(1).answered || network.reward_shop_pending())) {
        global_data.reward_shop_from_entry = false;
        return Screens::REWARD_SHOP;
    }
    network.reset_medals();
    return Screens::GAME_OVER;
}

void TotalResultScreen::on_screen_start() {
    Screen::on_screen_start();
    tally.reset();
    wait_since = get_current_ms();
}

void TotalResultScreen::start() {
    for (int i = 0; i < 2; i++) {
        nameplates[i].reset();
        charas[i].reset();
        if (!network.don_seat(i).answered) {
            continue;
        }
        const PlayerNum seat = i == 0 ? PlayerNum::P1 : PlayerNum::P2;
        auto pd = scores_manager.get_player_data(get_player_id(seat));
        nameplates[i].emplace(pd ? pd->username : "", pd ? pd->title : "", seat, pd ? pd->dan : -1,
                              pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
        charas[i] = make_chara_from_player_data(pd ? &*pd : nullptr, i == 1);
        if (pd) {
            charas[i]->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
            charas[i]->apply_face(pd->chara_face_index);
        }
        charas[i]->set_anim(AnimIndex::DON_GENERAL_LOOP);
    }
    tally = std::make_unique<TotalResult>();
}

Screens TotalResultScreen::on_screen_end(Screens next_screen) {
    tally.reset();
    for (int i = 0; i < 2; i++) {
        nameplates[i].reset();
        charas[i].reset();
    }
    clear_credit_tally();
    network.reset_don_points();
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> TotalResultScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    const double now = get_current_ms();
    allnet_indicator.update(now);
    if (!tally) {
        if (network.don_points_pending() && now - wait_since < kAnswerTimeoutMs) {
            return std::nullopt;
        }
        start();
        if (tally->unavailable()) {
            return on_screen_end(after_total_result());
        }
    }
    tally->update(now);
    for (auto& np : nameplates) {
        if (np) {
            np->update(now);
        }
    }
    for (auto& chara : charas) {
        if (chara) {
            chara->update(now);
        }
    }
    if (menu_confirm_pressed() && tally->skip()) {
        audio.play_sound("don", VolumePreset::SOUND);
    }
    if (tally->is_done()) {
        return on_screen_end(after_total_result());
    }
    return std::nullopt;
}

void TotalResultScreen::draw() {
    if (tally) {
        tally->draw();
        const float fade = tally->nameplate_fade();
        if (fade > 0) {
            auto [nx, ny] = tex.nameplate_xy("total_result");
            auto [cx, cy] = tex.chara_xy("total_result");
            for (int i = 0; i < 2; i++) {
                const float dx = i == 1 ? 640.0f * tex.screen_scale : 0.0f;
                if (charas[i]) {
                    charas[i]->draw(cx + (i == 1 ? 610.0f * tex.screen_scale : 0.0f), cy); // panels are 610 apart
                }
                if (nameplates[i]) {
                    nameplates[i]->draw(nx + dx, ny, fade);
                }
            }
        }
    }
    coin_overlay.draw();
    allnet_indicator.draw();
}
