#pragma once

#include <memory>
#include <optional>

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/nameplate.h"
#include "../objects/global/timer.h"
#include "../objects/reward_shop/reward_shop.h"

class RewardShopScreen : public Screen {
private:
    std::unique_ptr<RewardShop> shop;
    std::unique_ptr<Timer> timer;
    std::optional<Nameplate> nameplates[2];
    std::unique_ptr<Chara3D> charas[2];
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;
    bool from_entry = false;
    bool joined[2] = {false, false};
    bool time_up = false;
    bool lineup_requested = false;
    double wait_since = 0;

    void request_lineup();
    void start();
    void dress_chara(int seat, int type, int id);
    Screens next_screen() const;

public:
    RewardShopScreen() : Screen("reward_shop") {}
    void on_screen_start() override;
    Screens on_screen_end(Screens next) override;
    std::optional<Screens> update() override;
    void draw() override;
};
