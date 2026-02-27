#pragma once

#include "../enums.h"
#include "../../libs/animation.h"
#include "../../libs/texture.h"

class Judgment {
private:
    Judgments type;
    bool big;

    FadeAnimation* fade_animation_1;
    FadeAnimation* fade_animation_2;
    MoveAnimation* move_animation;
    TextureChangeAnimation* texture_animation;

public:
    Judgment(Judgments type, bool big);

    void update(double current_ms);

    void draw(float judge_x, float judge_y);

    bool is_finished() const;
};
