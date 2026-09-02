#pragma once

#include "../../libs/webcam.h"
#include "../../libs/animation.h"

class AttractCamera {
private:
    double start_ms = 0.0;
    bool finished = false;
    WebCamera camera;

    TextureChangeAnimation* live_icon_texture_change;
    BaseAnimation* scene_timer = nullptr;
public:
    AttractCamera();

    void update(double current_ms);

    void draw();

    bool is_finished();
};
