#pragma once

#include "../../libs/animation.h"

class Fireworks {
private:
    TextureChangeAnimation* explosion_anim;

public:
    Fireworks();

    void update(double current_ms);
    void draw();

    bool is_finished();
};
