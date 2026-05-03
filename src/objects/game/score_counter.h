#pragma once

#include "../../libs/animation.h"

class ScoreCounter {
private:
    int score;
    bool is_2p;
    TextStretchAnimation* stretch;

    void update_count(int score);

public:
    ScoreCounter(int score, bool is_2p);
    void update(double current_ms, int score);
    void draw(float y);

};
