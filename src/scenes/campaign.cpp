#include "campaign.h"

#include "../libs/file/scores.h"
#include "../libs/input/input.h"
#include "../objects/entry/player.h"

void CampaignScreen::on_screen_start() {
    Screen::on_screen_start();
    event.reset();
    if (!global_data.campaign.empty()) {
        std::vector<int> seats;
        for (const PlayerNum seat : global_data.entry_resume_seats) {
            if (seat != PlayerNum::ALL) {
                seats.push_back(static_cast<int>(seat));
            }
        }
        event = std::make_unique<CampaignEvent>(global_data.campaign, seats);
        if (event->unavailable()) {
            event.reset();
        }
    }
}

Screens CampaignScreen::on_screen_end(Screens next_screen) {
    event.reset();
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
            const auto& net = global_data.config->network;
            const std::string& code = id == scores_manager.player_1 ? net.access_code_1 : net.access_code_2;
            if (!code.empty()) {
                network.unlock_campaign_costume(code, cos_index);
            }
            save_player_costume(*pd);
            (id == scores_manager.player_1 ? scores_manager.player_1_data : scores_manager.player_2_data) = *pd;
        }
    }
}

void CampaignScreen::handle_input() {
    const bool two =
        global_data.entry_resume_seats[0] != PlayerNum::ALL && global_data.entry_resume_seats[1] != PlayerNum::ALL;
    for (const PlayerNum seat : global_data.entry_resume_seats) {
        if (seat == PlayerNum::ALL) {
            continue;
        }
        const PlayerNum keys = two ? seat : PlayerNum::ALL;
        const int player = static_cast<int>(seat);
        if (menu_left_pressed(keys) && event->input(CampaignEvent::LEFT, player)) {
            audio.play_sound("kat", VolumePreset::SOUND);
        } else if (menu_right_pressed(keys) && event->input(CampaignEvent::RIGHT, player)) {
            audio.play_sound("kat", VolumePreset::SOUND);
        } else if (menu_confirm_pressed(keys) && event->input(CampaignEvent::DECIDE, player)) {
            audio.play_sound("don", VolumePreset::SOUND);
        }
        if (!two) {
            break;
        }
    }
}

std::optional<Screens> CampaignScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    const double now = get_current_ms();
    allnet_indicator.update(now);
    if (!event) {
        return on_screen_end(next());
    }
    event->update(now);
    handle_input();
    if (event->is_done()) {
        for (const PlayerNum seat : {PlayerNum::P1, PlayerNum::P2}) {
            global_data.kinotake_team[static_cast<int>(seat) - 1] = event->get_team(static_cast<int>(seat));
        }
        if (const int cos = event->get_reward(); cos >= 0) {
            give_reward(cos);
        }
        const auto& net = global_data.config->network;
        for (const PlayerNum seat : global_data.entry_resume_seats) {
            const std::string& code = seat == PlayerNum::P1 ? net.access_code_1 : net.access_code_2;
            if (seat == PlayerNum::ALL || code.empty()) {
                continue;
            }
            if (const int cos = event->get_unlock(static_cast<int>(seat)); cos >= 0) {
                network.unlock_campaign_costume(code, cos);
            }
        }
        return on_screen_end(next());
    }
    return std::nullopt;
}

void CampaignScreen::draw() {
    if (event) {
        event->draw();
    }
    coin_overlay.draw();
    allnet_indicator.draw();
}
