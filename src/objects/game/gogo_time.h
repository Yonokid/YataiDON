#pragma once

#include "../../libs/animation.h"

class GogoTime {
private:
    TextureResizeAnimation* fire_resize;
    TextureChangeAnimation* fire_change;

public:
    GogoTime();

    void update(double current_ms);
    void draw(float judge_x, float judge_y);
};
