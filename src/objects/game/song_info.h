#pragma once

#include "../../libs/animation.h"
#include "../../libs/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/ray.h"
#include "../../libs/text.h"

class SongNum {
private:
    OutlinedText* text;
public:
    float width;
    float height;
    SongNum() = default;
    SongNum(int song_num);

    void draw(float x, float y, float fade);
};

class SongInfo {
private:
    std::string song_name;
    int genre;
    FadeAnimation* fade;
    OutlinedText* song_title;
    SongNum* song_num;

public:
    SongInfo() = default;
    SongInfo(const std::string& song_name, int genre);

    void update(double current_ms);
    void draw();
};
