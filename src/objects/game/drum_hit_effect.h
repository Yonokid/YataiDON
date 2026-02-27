#pragma once

#include "../enums.h"
#include "../../libs/animation.h"
#include "../../libs/texture.h"

class DrumHitEffect {
private:
    DrumType type;
    Side side;
    FadeAnimation* fade;

public:
    DrumHitEffect(DrumType type, Side side);

    void update(double current_ms);

    void draw(float y);

    bool is_finished() const;

};
