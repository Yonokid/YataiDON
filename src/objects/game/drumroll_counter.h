#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class DrumrollCounter {
private:
    int drumroll_count;
    FadeAnimation* fade;
    TextStretchAnimation* stretch;

public:
    DrumrollCounter();

    void update_count(int count);

    void update(double current_ms, int count);

    void draw(float y);

    bool is_finished() const;
};
