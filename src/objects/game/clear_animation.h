#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include <string>
#include <vector>

class ClearAnimation {
private:
    bool is_2p;
    FadeAnimation* bachio_fade_in;
    TextureChangeAnimation* bachio_texture_change;
    TextureChangeAnimation* bachio_out;
    MoveAnimation* bachio_move_out;
    std::vector<FadeAnimation*> clear_separate_fade_in;
    std::vector<TextStretchAnimation*> clear_separate_stretch;
    FadeAnimation* clear_highlight_fade_in;
    bool draw_clear_full;
    std::string name;
    int frame;

public:
    ClearAnimation(bool is_2p);

    void update(double current_ms);
    void draw();
};
