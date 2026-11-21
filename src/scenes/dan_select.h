#pragma once

#include <future>
#include <memory>
#include <optional>
#include <sol/sol.hpp>
#include <string>

#include "../libs/screen.h"
#include "../objects/dan_select/dan_navigator.h"
#include "../objects/dan_select/dan_select_scene.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/indicator.h"
#include "../objects/global/nameplate.h"
#include "../objects/global/timer.h"
#include "../objects/song_select/dan_transition.h"
#include "../objects/song_select/modifier.h"

class DanSelectScreen : public Screen {
public:
    DanSelectScreen() : Screen("dan_select") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;

private:
    DanNavigator dan_navigator;
    std::optional<DanSelectScene> scene; // skin draw; empty if the skin has no dan_select.lua
    CoinOverlay coin_overlay;
    AllNetIcon allnet_indicator;
    std::unique_ptr<Indicator> indicator;
    SongSelectState state = SongSelectState::BROWSING;
    std::optional<DanTransition> dan_transition; // plays on leaving for song select

    std::unique_ptr<Chara3D> chara;
    Nameplate nameplate;

    enum ConfirmEntry {
        CONFIRM_OPTION = 0,
        CONFIRM_YES = 1,
        CONFIRM_NO = 2
    };
    int confirm_index = CONFIRM_NO;
    FadeAnimation* confirm_fade = nullptr;

    double confirm_opened_at = 0;

    PlayerData dan_player_data;
    std::optional<ModifierSelector> modifier_selector;

    double last_moved = 0;

    bool wheel_locked = false;
    double wheel_tick_epoch = 0;
    long long wheel_tick_seen = -1;

    std::unique_ptr<Timer> select_timer;
    bool timer_started = false;
    bool timer_fired = false;
    double screen_start_ms = 0;
    double intro_ms = 0;

    static constexpr double SCAN_TIMEOUT_MS = 20000.0;
    bool scan_ready = false;
    double scan_ready_ms = 0;
    double scan_begin_ms = 0;
    bool legacy_blocking = false;

    void publish_scan_state(double current_ms);

    std::optional<Screens> tick_timer(double current_ms);
    std::optional<Screens> start_dan_transition();
    std::future<void> dan_download;
    void start_dan_download();
    void open_confirm(double current_ms);
    bool back_requested = false;

    void handle_input_browsing(double current_ms);
    std::optional<Screens> handle_input_selected();
};
