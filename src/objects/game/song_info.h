#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/ray.h"
#include "../../libs/utils.h"
#include <string>

class SongInfo {
private:
    std::string song_name;
    int genre;
    FadeAnimation* fade;
    // OutlinedText* song_title; // TODO: Implement OutlinedText class

public:
    SongInfo(const std::string& song_name, int genre);

    void update(double current_ms);
    void draw();
};
