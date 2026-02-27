#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class ScoreCounter {
private:
    int score;
    TextStretchAnimation* stretch;

    void update_count(int score);

public:
    ScoreCounter(int score);
    void update(double current_ms, int score);
    void draw(float y);

};
