#pragma once
#include "song_select.h"

class SongSelect2PScreen : public SongSelectScreen {
private:
    std::unique_ptr<SongSelectPlayer> player_2;

    void handle_input_browsing(double current_ms) override;
    void handle_input_selecting() override;
    void select_song(SongBox* song) override;
    void draw_overlays() override;

protected:
    Screens get_game_screen_target() override { return Screens::GAME_2P; }

public:
    SongSelect2PScreen() : SongSelectScreen("song_select") {}

    void on_screen_start() override;
    std::optional<Screens> update() override;
    void draw() override;
};
