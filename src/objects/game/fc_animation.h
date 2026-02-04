#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include <string>
#include <vector>

class FCAnimation {
private:
    bool is_2p;
    FadeAnimation* bachio_fade_in;
    TextureChangeAnimation* bachio_texture_change;
    TextureChangeAnimation* bachio_out;
    MoveAnimation* bachio_move_out;
    std::vector<FadeAnimation*> clear_separate_fade_in;
    std::vector<TextStretchAnimation*> clear_separate_stretch;
    FadeAnimation* clear_highlight_fade_in;
    MoveAnimation* fc_highlight_up;
    FadeAnimation* fc_highlight_fade_out;
    MoveAnimation* bachio_move_out_2;
    MoveAnimation* bachio_move_up;
    FadeAnimation* fan_fade_in;
    TextureChangeAnimation* fan_texture_change;
    bool draw_clear_full;
    std::string name;
    int frame;

public:
    FCAnimation(bool is_2p);

    void update(double current_ms);
    void draw();
};
