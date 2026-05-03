#pragma once

#include "../../libs/text.h"

class SongNum {
private:
    std::unique_ptr<OutlinedText> text;
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
    std::unique_ptr<OutlinedText> song_title;
    std::unique_ptr<SongNum> song_num;

public:
    SongInfo() = default;
    SongInfo(const std::string& song_name, int genre);

    void update(double current_ms);
    void draw();
};
