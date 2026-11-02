#include "campaign.h"

#include "../libs/file/scores.h"
#include "../libs/input/input.h"
#include "../objects/entry/player.h"

void CampaignScreen::on_screen_start() {
    Screen::on_screen_start();
    mac_photo.reset();
    if (global_data.campaign == "mac") {
        mac_photo = std::make_unique<MacPhoto>();
        if (mac_photo->unavailable()) {
            mac_photo.reset();
        }
    }
    if (mac_photo && audio.has_sound("bgm")) {
        audio.play_sound("bgm", VolumePreset::MUSIC);
    }
}

Screens CampaignScreen::on_screen_end(Screens next_screen) {
    audio.stop_sound("bgm");
    mac_photo.reset();
    return Screen::on_screen_end(next_screen);
}

void CampaignScreen::give_reward(int cos_index) {
    for (const std::string& id : {scores_manager.player_1, scores_manager.player_2}) {
        if (id.empty()) {
            continue;
        }
        if (auto pd = scores_manager.get_player_data(id)) {
            pd->chara_cos_index = cos_index;
            pd->chara_is_costume = true;
            save_player_costume(*pd);
            (id == scores_manager.player_1 ? scores_manager.player_1_data : scores_manager.player_2_data) = *pd;
        }
    }
}

std::optional<Screens> CampaignScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    const double now = get_current_ms();
    allnet_indicator.update(now);
    if (!mac_photo) {
        return on_screen_end(next());
    }
    mac_photo->update(now);
    if (menu_confirm_pressed() && mac_photo->input_decide()) {
        audio.play_sound("don", VolumePreset::SOUND);
    }
    if (mac_photo->is_done()) {
        if (const int cos = mac_photo->get_reward(); cos >= 0) {
            give_reward(cos);
        }
        return on_screen_end(next());
    }
    return std::nullopt;
}

void CampaignScreen::draw() {
    if (mac_photo) {
        mac_photo->draw();
    }
    coin_overlay.draw();
    allnet_indicator.draw();
}
