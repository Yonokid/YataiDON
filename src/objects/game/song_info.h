#pragma once

#include "../../libs/file/script.h"
#include "../../libs/gfx/text.h"

class SongNum {
private:
    std::unique_ptr<OutlinedText> text;

public:
    float width = 0.0f;
    float height = 0.0f;
    SongNum() = default;
    SongNum(int song_num);

    void draw(float x, float y, float fade);
};

// The skin's SongInfo (game/song_info.lua) draws the title, subtitle, song number and genre plate.
class SongInfo : public LuaScript {
private:
    sol::protected_function fn_update, fn_draw;

public:
    SongInfo() = default;
    SongInfo(const std::string& song_name, const std::string& subtitle, bool show_subtitle, int genre, int song_num,
             const std::string& genre_label = "");

    void update(double current_ms);
    // (dx, dy) moves the whole block (the AI battle shows it at the bottom right)
    void draw(float dx = 0.0f, float dy = 0.0f);
};
