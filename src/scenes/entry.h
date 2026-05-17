#pragma once

#include "../libs/screen.h"
#include "../objects/entry/box_manager.h"
#include "../objects/entry/entry_script.h"
#include "../objects/entry/player.h"
#include "../objects/global/nameplate.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/entry_overlay.h"
#include "../objects/global/timer.h"
#include "../objects/global/chara_3d.h"


enum class EntryState {
    SELECT_SIDE = 0,
    SELECT_MODE = 1
};

class EntryScreen : public Screen {
private:
    int side;
    bool is_2p;
    std::unique_ptr<BoxManager> box_manager;
    EntryState state;

    std::unique_ptr<EntryScript> lua_entry;
    Nameplate nameplate;
    CoinOverlay coin_overlay;
    AllNetIcon allnet_indicator;
    EntryOverlay entry_overlay;
    std::unique_ptr<Timer> timer;

    bool screen_init;
    std::unique_ptr<Chara3D> chara;
    bool announce_played;
    std::vector<std::unique_ptr<EntryPlayer>> players;

    void draw_background();
    void draw_side_select(float fade);
    void draw_player_drum();
    void draw_mode_select();
    std::optional<Screens> handle_input();

public:
    EntryScreen() : Screen("entry") {
    }
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
