#pragma once

#include "../../libs/texture.h"
#include "../../libs/text.h"

class Transition {
private:
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
    void update(double current_ms);
    void draw();

    bool is_finished();
};
