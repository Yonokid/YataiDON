#pragma once

#include "../enums.h"
#include "../../libs/animation.h"
#include "../../libs/texture.h"

class LaneHitEffect {
private:
    bool is_2p;
    DrumType type;
    Judgments judgment;
    FadeAnimation* fade;

public:
    LaneHitEffect(DrumType type, Judgments judgment, bool is_2p);

    void update(double current_ms);

    void draw();

    bool is_finished() const;
};
