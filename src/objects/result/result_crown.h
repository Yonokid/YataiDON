#pragma once

#include "../../libs/texture.h"
#include "../../libs/audio.h"

enum CrownType {
    CROWN_CLEAR = 0,
    CROWN_FC = 1,
    CROWN_DFC = 2
};

class ResultCrown {
private:
    TextureResizeAnimation* resize;
    TextureResizeAnimation* resize_fix;
    FadeAnimation* white_fadein;
    TextureChangeAnimation* gleam;
    FadeAnimation* fadein;

    bool sound_played = false;
    bool is_2p = false;

public:
    ResultCrown(bool is_2p);

    void update(double current_ms);

    void draw(CrownType crown_type);
};
