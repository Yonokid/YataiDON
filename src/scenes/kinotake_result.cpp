#include "kinotake_result.h"

#include "../libs/file/scores.h"
#include "../libs/input/input.h"

static constexpr double kAnswerTimeoutMs = 5000.0;

void KinotakeResultScreen::on_screen_start() {
    Screen::on_screen_start();
    tally.reset();
    wait_since = get_current_ms();
}

void KinotakeResultScreen::start() {
    const KinotakeTally& t = network.kinotake_tally();
    for (int i = 0; i < 2; i++) {
        nameplates[i].reset();
        if (t.seat[i].plays == 0) {
            continue;
        }
        const PlayerNum seat = i == 0 ? PlayerNum::P1 : PlayerNum::P2;
        auto pd = scores_manager.get_player_data(get_player_id(seat));
        nameplates[i].emplace(pd ? pd->username : "", pd ? pd->title : "", seat, pd ? pd->dan : -1,
                              pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    }
    tally = std::make_unique<KinotakeResult>(t);
}

Screens KinotakeResultScreen::on_screen_end(Screens next_screen) {
    tally.reset();
    nameplates[0].reset();
    nameplates[1].reset();
    network.reset_kinotake_tally();
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> KinotakeResultScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    const double now = get_current_ms();
    allnet_indicator.update(now);
    if (!tally) {
        if (network.kinotake_pending() && now - wait_since < kAnswerTimeoutMs) {
            return std::nullopt;
        }
        if (!network.kinotake_tally().any()) { // nothing was answered: nothing to show
            return on_screen_end(next());
        }
        start();
        if (tally->unavailable()) {
            return on_screen_end(next());
        }
    }
    tally->update(now);
    for (auto& np : nameplates) {
        if (np) {
            np->update(now);
        }
    }
    if (menu_confirm_pressed() && tally->skip()) {
        audio.play_sound("don", VolumePreset::SOUND);
    }
    if (tally->is_done()) {
        return on_screen_end(next());
    }
    return std::nullopt;
}

void KinotakeResultScreen::draw() {
    if (tally) {
        tally->draw();
        auto [nx, ny] = tex.nameplate_xy("kinotake_result");
        for (int i = 0; i < 2; i++) {
            if (!nameplates[i]) {
                continue;
            }
            auto [dy, fade] = tally->nameplate(i);
            if (fade > 0) {
                nameplates[i]->draw(nx + (i == 1 ? 640.0f * tex.screen_scale : 0.0f), ny + dy, fade);
            }
        }
    }
    coin_overlay.draw();
    allnet_indicator.draw();
}
