#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class ScoreCounter {
private:
    bool is_2p;
    int score;
    TextStretchAnimation* stretch;

    void update_count(int score);

public:
    ScoreCounter(bool is_2p, int score);
    void update(double current_ms, int score);
    void draw();

};
