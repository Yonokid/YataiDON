#pragma once

#include <memory>
#include <string>
#include <vector>

#include "box.h"

class BoxManager {
private:
    std::vector<Screens> box_locations;
    std::vector<std::unique_ptr<Box>> boxes;
    int num_boxes = 0;
    int selected_box_index{0};
    FadeAnimation* fade_out;
    bool is_2p;
    bool is_vertical = false;
    int dan_box_index = -1;
    bool dan_available = false;
    std::string dan_text;

    void build_board_list();
    void change_board_list();

public:
    bool costume_menu_open{false};
    bool shop_request{false};
    bool bncoin_request{false};
    PlayerNum opening_player = PlayerNum::P1;

    explicit BoxManager(bool two_player = false);
    bool check_board_list_change() const;
    bool selection_allowed() const;
    bool is_disabled(int index) const; // greyed out: shown but not selectable
    void select_box();
    void start_selected();
    bool is_box_selected() const;
    bool is_finished() const;
    bool is_costume_box() const;
    void open_costume_menu(PlayerNum player_num = PlayerNum::P1);
    Screens selected_box() const;
    void move_left();
    void move_right();
    void update(double current_time_ms, bool two_player);
    void draw();
};
