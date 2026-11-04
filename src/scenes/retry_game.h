#pragma once

#include <optional>

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/result/retry_game.h"

class RetryGameScreen : public Screen {
private:
    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;
    std::optional<RetryGameScene> scene;
    std::unique_ptr<Chara3D> chara;
    std::string chara_phase;

    float chara_target = 0.0f;
    float chara_progress = 0.0f;
    double last_ms = 0;

protected:
    virtual Screens select_screen() const { return Screens::SONG_SELECT; }

public:
    static constexpr int needed_hits = 30;

    RetryGameScreen() : Screen("retry_game") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
