#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/audio.h"

class ComboAnnounce {
private:
    PlayerNum player_num;
    bool is_2p;
    int combo;
    float wait;
    FadeAnimation* fade;
    bool audio_played;

public:
    bool is_finished;

    ComboAnnounce(int combo, double current_ms, PlayerNum player_num, bool is_2p);

    void update(double current_ms);
    void draw();
};
