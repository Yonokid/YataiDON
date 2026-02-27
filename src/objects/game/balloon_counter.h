#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class BalloonCounter {
private:
    int balloon_count;
    int balloon_total;
    bool is_popped;
    TextStretchAnimation* stretch;
    FadeAnimation* fade;
public:
    BalloonCounter(int count);

    void update_count(int count);

    void update(double current_ms, int count);

    void draw(float y);

    bool is_finished() const;
};
