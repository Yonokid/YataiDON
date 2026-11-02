#pragma once

#include <memory>

#include "../libs/screen.h"
#include "../objects/campaign/mac_photo.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"

class CampaignScreen : public Screen {
private:
    std::unique_ptr<MacPhoto> mac_photo;
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;

    Screens next() const { return static_cast<Screens>(global_data.campaign_next); }
    void give_reward(int cos_index);

public:
    CampaignScreen() : Screen("campaign") {}
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
