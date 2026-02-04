#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class BalloonCounter {
private:
    PlayerNum player_num;
    bool is_2p;
    int balloon_count;
    int balloon_total;
    bool is_popped;
    TextStretchAnimation* stretch;
    FadeAnimation* fade;
public:
    BalloonCounter(int count, PlayerNum player_num, bool is_2p);

    void update_count(int count);

    void update(double current_ms, int count, bool popped);

    void draw();

    bool is_finished() const;
};
