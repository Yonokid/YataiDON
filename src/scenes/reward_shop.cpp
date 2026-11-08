#include "reward_shop.h"

#include "../libs/file/scores.h"
#include "../libs/file/script.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"

static constexpr double kAnswerTimeoutMs = 5000.0;
static constexpr int kShopSeconds = 60; // the arcade's limit is C++ data; Entry's select timer length

static const std::string& seat_code(int seat) {
    return seat == 0 ? global_data.config->network.access_code_1 : global_data.config->network.access_code_2;
}

static PlayerNum seat_num(int seat) {
    return seat == 0 ? PlayerNum::P1 : PlayerNum::P2;
}

void RewardShopScreen::on_screen_start() {
    Screen::on_screen_start();
    from_entry = global_data.reward_shop_from_entry;
    global_data.reward_shop_from_entry = false;
    for (int i = 0; i < 2; i++) {
        joined[i] = from_entry ? (global_data.entry_resume_seats[0] == seat_num(i) ||
                                  global_data.entry_resume_seats[1] == seat_num(i))
                               : global_data.credit_seats[i];
    }
    shop.reset();
    timer.reset();
    time_up = false;
    lineup_requested = false;
    wait_since = get_current_ms();
    if (from_entry) {
        network.reset_medals();
        request_lineup();
    }
}

void RewardShopScreen::request_lineup() {
    for (int i = 0; i < 2; i++) {
        if (joined[i] && !seat_code(i).empty()) {
            network.request_reward_shop(i, seat_code(i));
        }
    }
}

Screens RewardShopScreen::next_screen() const {
    return from_entry ? Screens::ENTRY : Screens::GAME_OVER;
}

void RewardShopScreen::dress_chara(int seat, int type, int id) {
    auto pd = scores_manager.get_player_data(get_player_id(seat_num(seat)));
    if (!pd) {
        return;
    }
    if (type == 2) {
        pd->chara_is_costume = true;
        pd->chara_cos_index = id;
    } else if (type == 3 || type == 4) {
        pd->chara_is_costume = false;
        (type == 3 ? pd->chara_head_index : pd->chara_body_index) = id;
    } else if (type == 5) {
        pd->chara_acce_index = id;
    }
    charas[seat] = make_chara_from_player_data(&*pd, seat == 1);
    charas[seat]->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
    charas[seat]->apply_face(pd->chara_face_index);
    charas[seat]->set_anim(AnimIndex::DON_GENERAL_LOOP);
}

void RewardShopScreen::start() {
    for (int i = 0; i < 2; i++) {
        nameplates[i].reset();
        charas[i].reset();
        if (!network.shop_seat(i).answered) {
            continue;
        }
        auto pd = scores_manager.get_player_data(get_player_id(seat_num(i)));
        nameplates[i].emplace(pd ? pd->username : "", pd ? pd->title : "", seat_num(i), pd ? pd->dan : -1,
                              pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
        dress_chara(i, -1, -1);
    }
    RewardShop::Hooks hooks;
    hooks.buy = [](int seat, int type, int id) { network.buy_shop_good(seat, seat_code(seat), type, id); };
    hooks.preview = [this](int seat, int type, int id) { dress_chara(seat, type, id); };
    hooks.draw_chara = [this](int seat, float x, float y) {
        if (charas[seat]) {
            charas[seat]->draw(x, y);
        }
    };
    shop = std::make_unique<RewardShop>(!from_entry, joined, hooks);
}

Screens RewardShopScreen::on_screen_end(Screens next) {
    shop.reset();
    timer.reset();
    for (int i = 0; i < 2; i++) {
        nameplates[i].reset();
        charas[i].reset();
    }
    network.reset_medals();
    return Screen::on_screen_end(next);
}

std::optional<Screens> RewardShopScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    const double now = get_current_ms();
    allnet_indicator.update(now);
    if (!shop) {
        if (network.reward_shop_pending() && now - wait_since < kAnswerTimeoutMs) {
            return std::nullopt;
        }
        // Reward time fetches the lineup only once the claim has landed, for the fresh balance
        if (!from_entry && !lineup_requested) {
            lineup_requested = true;
            wait_since = now;
            request_lineup();
            return std::nullopt;
        }
        if (!network.shop_seat(0).answered && !network.shop_seat(1).answered) {
            return end_or_game_over(next_screen());
        }
        start();
        if (shop->unavailable()) {
            return end_or_game_over(next_screen());
        }
    }
    shop->update(now);
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
    if (!timer && shop->in_shop()) {
        timer = std::make_unique<Timer>(kShopSeconds, now, [this] { time_up = true; });
    }
    if (timer) {
        timer->update(now);
    }
    if (time_up) {
        time_up = false;
        shop->time_up();
    }
    if (auto bought = network.take_shop_buy_result()) {
        shop->on_buy(*bought);
    }
    const bool both = network.shop_seat(0).answered && network.shop_seat(1).answered;
    for (int i = 0; i < 2; i++) {
        if (!network.shop_seat(i).answered || !shop->wants_input(i)) {
            continue;
        }
        const PlayerNum seat = both ? seat_num(i) : PlayerNum::ALL;
        if (menu_left_pressed(seat)) {
            shop->input(i, RewardShop::LEFT);
        } else if (menu_right_pressed(seat)) {
            shop->input(i, RewardShop::RIGHT);
        } else if (menu_confirm_pressed(seat)) {
            shop->input(i, RewardShop::DECIDE);
        }
    }
    if (shop->is_done()) {
        return end_or_game_over(next_screen());
    }
    return std::nullopt;
}

void RewardShopScreen::draw() {
    if (shop) {
        shop->draw();
        for (int i = 0; i < 2; i++) {
            if (nameplates[i]) {
                auto [nx, ny] = tex.nameplate_xy(i == 1 ? "reward_shop_2p" : "reward_shop");
                nameplates[i]->draw(nx, ny, 1.0f);
            }
        }
    }
    if (timer) {
        timer->draw();
    }
    coin_overlay.draw();
    allnet_indicator.draw();
}
