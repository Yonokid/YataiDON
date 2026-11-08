#pragma once
#include "song_select.h"

class SongSelect2PScreen : public SongSelectScreen {
private:
    void handle_input_browsing(double current_ms) override;
    void handle_input_selecting() override;
    void draw_overlays() override;

protected:
    std::unique_ptr<SongSelectPlayer> player_2;

    void select_song(SongBox* song) override;
    void on_select_timeout() override;
    SongSelectPlayer* second_player() override { return player_2.get(); }
    bool is_2p_screen() override { return true; }
    Screens get_game_screen_target() override { return Screens::GAME_2P; }

    bool hides_dan() override { return true; }

public:
    bool two_player_audio() const override { return true; }

    SongSelect2PScreen() : SongSelectScreen("song_select") {}

    void on_screen_start() override;
    std::optional<Screens> update() override;
    void draw() override;
};
