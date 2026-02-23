#pragma once

#include "../../libs/texture.h"
#include "../../libs/text.h"

class Nameplate {
private:
    OutlinedText* name;
    OutlinedText* title;
    TextureChangeAnimation* rainbow_animation;

    int dan_index;
    PlayerNum player_num;
    bool is_gold;
    bool is_rainbow;
    int title_bg;

public:

    Nameplate() = default;
    Nameplate(std::string name, std::string title, PlayerNum player_num, int dan, bool is_gold, bool is_rainbow, int title_bg);

    void update(double current_ms);

    void draw(float x, float y, float fade = 1.0);
};
