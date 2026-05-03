#pragma once

#include "../../libs/global_data.h"
#include "../../libs/text.h"

class NeiroSelector {
private:
    PlayerNum player_num;
    int selected_sound;
    int direction;
    std::vector<std::string> sounds;
    std::string curr_sound;

    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;
    MoveAnimation* move_sideways;
    FadeAnimation* fade_sideways;

    std::unique_ptr<OutlinedText> text;
    std::unique_ptr<OutlinedText> text_2;

    void load_sound();

public:
    bool is_finished;
    bool is_confirmed;
    MoveAnimation* move;

    NeiroSelector(PlayerNum player_num);
    void update(double current_ms);
    void left();
    void right();
    void confirm();
    void draw();
};
