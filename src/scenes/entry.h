#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/audio.h"
#include "../../libs/input.h"
#include "../../libs/screen.h"
#include "../../libs/text.h"
#include "../objects/global/nameplate.h"
#include "../objects/global/timer.h"
#include "../objects/global/indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/entry_overlay.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/entry/box_manager.h"
#include "../objects/entry/player.h"
#include <vector>
#include <optional>

enum class EntryState {
    SELECT_SIDE = 0,
    SELECT_MODE = 1
};

class EntryScreen : public Screen {
private:
    int side;
    bool is_2p;
    BoxManager* box_manager;
    EntryState state;

    Nameplate nameplate;
    CoinOverlay coin_overlay;
    AllNetIcon allnet_indicator;
    EntryOverlay entry_overlay;
    Timer* timer;

    bool screen_init;
    FadeAnimation* side_select_fade;
    FadeAnimation* bg_flicker;
    //Chara2D* chara;
    bool announce_played;
    std::vector<EntryPlayer*> players;

    OutlinedText* text_cancel;
    OutlinedText* text_question;

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
