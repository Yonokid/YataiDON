#pragma once

#include "../../libs/text.h"
#include "../../libs/script.h"

class Transition : public LuaScript {
private:
    sol::protected_function fn_update, fn_draw_bg, fn_draw_info;

    int  dan_color   = -1;
    double dan_start_ms = 0.0;
    std::unique_ptr<OutlinedText> dan_rank_text;
    void draw_dan(float total_offset);

    bool is_second;
    std::unique_ptr<OutlinedText> title;
    std::unique_ptr<OutlinedText> subtitle;
    std::optional<ray::Texture2D> loading_graphic;
    void draw_song_info();
    void draw_default(float total_offset);

    MoveAnimation* rainbow_up;
    MoveAnimation* mini_up;
    MoveAnimation* chara_down;
    FadeAnimation* song_info_fade;
    FadeAnimation* song_info_fade_out;
public:

    Transition(const std::string& title, const std::string& subtitle, bool is_second);
    ~Transition();
    void start();
    void add_loading_graphic(const std::string& path);
    void set_dan(int color, const std::string& rank_name);
    void update(double current_ms);
    void draw();

    bool is_finished();
};
