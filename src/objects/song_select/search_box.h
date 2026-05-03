#pragma once

#include "../../libs/animation.h"
#include "../../libs/ray.h"

class SearchBox {
private:
    FadeAnimation* diff_fade_in;
    TextureChangeAnimation* bg_resize;
    ray::Font font;

public:
    std::string current_search;
    SearchBox();
    void update(double current_ms);
    void draw();
};
