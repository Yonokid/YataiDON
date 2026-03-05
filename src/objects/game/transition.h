#pragma once

#include "../../libs/texture.h"
#include "../../libs/text.h"

class Transition {
private:
    bool is_second;
    OutlinedText* title;
    OutlinedText* subtitle;
    void draw_song_info();

    MoveAnimation* rainbow_up;
    MoveAnimation* mini_up;
    MoveAnimation* chara_down;
    FadeAnimation* song_info_fade;
    FadeAnimation* song_info_fade_out;
public:

    Transition(const std::string& title, const std::string& subtitle, bool is_second);
    void start();
    void update(double current_ms);
    void draw();

    bool is_finished();
};
