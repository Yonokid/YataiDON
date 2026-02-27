#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"

class Fireworks {
private:
    TextureChangeAnimation* explosion_anim;

public:
    Fireworks();

    void update(double current_ms);
    void draw();
};
