#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/audio.h"

class ComboAnnounce {
private:
    PlayerNum player_num;
    int combo;
    double wait;
    FadeAnimation* fade;
    bool audio_played;

public:
    bool is_finished;

    ComboAnnounce(int combo, double current_ms, PlayerNum player_num);

    void update(double current_ms);
    void draw(float y);
};
