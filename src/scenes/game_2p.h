#pragma once

#include "game.h"

class Game2PScreen : public GameScreen {
private:
    std::optional<SongParser> parser_2p;

public:
    Game2PScreen() : GameScreen("game") {}

    PlayerNum scene_player_num() const override { return PlayerNum::TWO_PLAYER; }

    void init_tja(fs::path song) override;
    std::optional<Screens> update() override;
};
