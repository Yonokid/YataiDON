#pragma once

#include <memory>
#include <optional>

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/nameplate.h"
#include "../objects/total_result/total_result.h"

Screens begin_total_result();

class TotalResultScreen : public Screen {
private:
    std::unique_ptr<TotalResult> tally;
    std::optional<Nameplate> nameplates[2];
    std::unique_ptr<Chara3D> charas[2];
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;
    double wait_since = 0;

    void start();

public:
    TotalResultScreen() : Screen("total_result") {}
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
