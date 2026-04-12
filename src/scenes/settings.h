#pragma once
#include "../libs/screen.h"
#include "../libs/audio.h"
#include "../libs/config.h"
#include "../libs/input.h"
#include "../objects/global/indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/settings/settings_box_manager.h"
#include <optional>

class SettingsScreen : public Screen {
private:
    SettingsBoxManager* box_manager;
    Indicator           indicator;
    CoinOverlay         coin_overlay;
    AllNetIcon          allnet_indicator;

    std::optional<Screens> handle_input();

public:
    SettingsScreen() : Screen("settings"), box_manager(nullptr),
                       indicator(Indicator::State::SELECT) {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
