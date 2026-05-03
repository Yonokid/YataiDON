#pragma once

#include "../enums.h"
#include "../../libs/animation.h"


class LaneHitEffect {
private:
    DrumType type;
    Judgments judgment;
    FadeAnimation* fade;

public:
    LaneHitEffect(DrumType type, Judgments judgment);

    void update(double current_ms);

    void draw(float y);

    bool is_finished() const;
};
