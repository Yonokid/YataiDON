#pragma once

#include <memory>

#include "../../libs/gfx/text.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/math/animation.h"
#include "../enums.h"

class BranchIndicator {
private:
    BranchDifficulty diff_2{BranchDifficulty::NORMAL};
    MoveAnimation* diff_down;
    MoveAnimation* diff_up;
    FadeAnimation* diff_fade;
    FadeAnimation* level_fade;
    TextureResizeAnimation* level_scale;
    int direction{1};
    TextureObject* t_expert_bg = nullptr;
    TextureObject* t_master_bg = nullptr;
    TextureObject* t_level_up = nullptr;
    TextureObject* t_level_down = nullptr;
    TextureObject* t_diff[3] = {}; // indexed by BranchDifficulty
    // the banners are blank art; their caption is skin text (branch_level_up / branch_level_down)
    std::unique_ptr<OutlinedText> caption_up;
    std::unique_ptr<OutlinedText> caption_down;
    float caption_dx = 0;
    float caption_dy = 0;

public:
    BranchDifficulty difficulty{BranchDifficulty::NORMAL};
    BranchIndicator();

    void update(double current_ms);
    void level_up(BranchDifficulty difficulty);
    void level_down(BranchDifficulty difficulty);
    void draw(float y);
};
