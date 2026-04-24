#pragma once
#include "song_select.h"

class PracticeSongSelectScreen : public SongSelectScreen {
protected:
    Screens get_game_screen_target() override { return Screens::GAME_PRACTICE; }

public:
    PracticeSongSelectScreen() : SongSelectScreen("song_select") {}
};
