#pragma once

#include "../../libs/file/scores.h"
#include "../../libs/gfx/text.h"
#include "../../libs/global_data.h"

// Display names for the hit sound sets, in folder order.
std::vector<std::string> read_neiro_names();

class NeiroSelector {
private:
    PlayerNum player_num;
    PlayerData* player;
    int selected_sound;
    int direction;
    std::vector<std::string> sounds;
    std::string curr_sound;

    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;
    MoveAnimation* move_sideways;
    FadeAnimation* fade_sideways;

    void load_sound();

    int change_seq = 0;
    int instance_id = 0;

public:
    const std::vector<std::string>& lua_names() const { return sounds; }
    int lua_index() const { return selected_sound; }
    int lua_id() const { return instance_id; }
    PlayerNum lua_player() const { return player_num; }
    // the sideways-slide animation of the name text: passed to the Lua panel each frame
    int lua_change_dir() const { return direction; }
    int lua_change_seq() const { return change_seq; }
    double lua_change_move() const { return move_sideways->attribute; }
    double lua_change_fade() const { return fade_sideways->attribute; }
    bool lua_change_active() const { return !move_sideways->is_finished; }
    double lua_arrow_fade() const { return blue_arrow_fade->attribute; }
    double lua_arrow_move() const { return blue_arrow_move->attribute; }

    bool is_finished;
    bool is_confirmed;
    MoveAnimation* move;
    MoveAnimation* move_out = nullptr;

    NeiroSelector(PlayerNum player_num, PlayerData* player);
    void update(double current_ms);
    void left();
    void right();
    void confirm();
};
