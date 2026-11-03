#pragma once

#include <memory>
#include <optional>

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/nameplate.h"
#include "../objects/kinotake_result/kinotake_result.h"

class KinotakeResultScreen : public Screen {
private:
    std::unique_ptr<KinotakeResult> tally;
    std::optional<Nameplate> nameplates[2];
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;
    double wait_since = 0;

    Screens next() const { return static_cast<Screens>(global_data.kinotake_next); }
    void start();

public:
    KinotakeResultScreen() : Screen("kinotake_result") {}
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
