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
    int dan_box_index = -1;
    bool dan_available = false;
    std::string dan_text;

    void build_board_list();
    void change_board_list();

public:
    bool costume_menu_open;
    PlayerNum opening_player = PlayerNum::P1;


    explicit BoxManager(bool two_player = false);
    bool check_board_list_change() const;
    bool selection_allowed() const;
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
