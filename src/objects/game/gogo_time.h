#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class GogoTime {
private:
    bool is_2p;
    TextureChangeAnimation* explosion_anim;
    TextureResizeAnimation* fire_resize;
    TextureChangeAnimation* fire_change;

public:
    GogoTime(bool is_2p);

    void update(double current_ms);
    void draw(float judge_x, float judge_y);
};
