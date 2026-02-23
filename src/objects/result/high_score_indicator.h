#pragma once

#include "../../libs/texture.h"

class HighScoreIndicator {
private:
    bool is_2p;
    int score_diff;
    MoveAnimation* move;
    FadeAnimation* fade;
public:
    HighScoreIndicator(int old_score, int new_score, bool is_2p);

    void update(double current_ms);

    void draw();
};
