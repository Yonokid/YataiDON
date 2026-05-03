#pragma once

#include "../../libs/animation.h"

class UraSwitchAnimation {
private:
    TextureChangeAnimation* texture_change;
    FadeAnimation* fade_out;
public:
    UraSwitchAnimation();

    void start(bool is_backwards);

    void update(double current_ms);

    void draw();
};
