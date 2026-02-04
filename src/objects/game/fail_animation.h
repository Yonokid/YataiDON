#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include <string>

class FailAnimation {
private:
    bool is_2p;
    FadeAnimation* bachio_fade_in;
    TextureChangeAnimation* bachio_texture_change;
    MoveAnimation* bachio_fall;
    MoveAnimation* bachio_move_out;
    FadeAnimation* bachio_boom_fade_in;
    TextureResizeAnimation* bachio_boom_scale;
    MoveAnimation* bachio_up;
    MoveAnimation* bachio_down;
    FadeAnimation* text_fade_in;
    std::string name;
    int frame;

public:
    FailAnimation(bool is_2p);

    void update(double current_ms);
    void draw();
};
