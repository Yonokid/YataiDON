#pragma once

#include "box.h"

class BoxManager {
private:
    std::vector<Screens> box_locations;
    std::vector<std::unique_ptr<Box>> boxes;
    int num_boxes;
    int selected_box_index;
    FadeAnimation* fade_out;
    bool is_2p;
    bool is_vertical;

public:
    bool costume_menu_open;
    PlayerNum opening_player = PlayerNum::P1;

    BoxManager();
    void select_box();
    bool is_box_selected();
    bool is_finished();
    bool is_costume_box();
    void open_costume_menu(PlayerNum player_num = PlayerNum::P1);
    Screens selected_box();
    void move_left();
    void move_right();
    void update(double current_time_ms, bool is_2p);
    void draw();
};
