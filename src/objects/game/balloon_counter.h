#pragma once

#include "../../libs/animation.h"

class BalloonCounter {
private:
    int balloon_count;
    int balloon_total;
    bool is_popped;
    bool is_2p;
    TextStretchAnimation* stretch;
    FadeAnimation* fade;
public:
    BalloonCounter(int count, bool is_2p);

    void update_count(int count);

    void update(double current_ms, int count);

    void draw(float y);

    bool is_finished() const;
};
