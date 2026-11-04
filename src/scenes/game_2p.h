#pragma once

#include "game.h"

class Game2PScreen : public GameScreen {
private:
    std::optional<SongParser> parser_2p;

    void release_chart_resources() override;

    PlayerNum restart_player_num() const override { return PlayerNum::P1; }

protected:
    virtual Screens select_screen() const { return Screens::SONG_SELECT_2P; }
    virtual Screens result_screen() const { return Screens::RESULT_2P; }

public:
    bool two_player_audio() const override { return true; }

    Game2PScreen() : GameScreen("game") {}

    PlayerNum scene_player_num() const override { return PlayerNum::TWO_PLAYER; }

    void initialize_chart(const fs::path& song) override;
    std::optional<Screens> update() override;
};
