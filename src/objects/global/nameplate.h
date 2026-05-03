#pragma once

#include "../../libs/text.h"
#include "../../libs/global_data.h"

class Nameplate {
private:
    std::unique_ptr<OutlinedText> name;
    std::unique_ptr<OutlinedText> title;
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
