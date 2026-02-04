#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../enums.h"

class BranchIndicator {
private:
    bool is_2p;
    BranchDifficulty difficulty;
    BranchDifficulty diff_2;
    MoveAnimation* diff_down;
    MoveAnimation* diff_up;
    FadeAnimation* diff_fade;
    FadeAnimation* level_fade;
    TextureResizeAnimation* level_scale;
    int direction;

public:
    BranchIndicator(bool is_2p);

    void update(double current_ms);
    void level_up(BranchDifficulty difficulty);
    void level_down(BranchDifficulty difficulty);
    void draw();
};
