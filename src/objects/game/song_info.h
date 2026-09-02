#pragma once

#include "../../libs/text.h"

class SongNum {
private:
    std::unique_ptr<OutlinedText> text;
public:
    float width;
    float height;
    SongNum() = default;
    SongNum(int song_num, float outline_override = -1.0f);
    SongNum(int value, const std::string& config_key);

    void draw(float x, float y, float fade);
};

class SongInfo {
private:
    std::string song_name;
    int genre;
    FadeAnimation* fade;
    std::unique_ptr<OutlinedText> song_title;
    std::unique_ptr<OutlinedText> song_subtitle;
    std::unique_ptr<SongNum> song_num;
    std::unique_ptr<SongNum> song_max;

public:
    SongInfo() = default;
    SongInfo(const std::string& song_name, const std::string& subtitle, bool show_subtitle, int genre, int song_num, int song_total = 0);

    void update(double current_ms);
    void draw();
};
