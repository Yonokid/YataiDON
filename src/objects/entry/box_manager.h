#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/text.h"
#include "box.h"
#include <vector>

class BoxManager {
private:
    std::vector<Screens> box_locations;
    std::vector<std::unique_ptr<Box>> boxes;
    int num_boxes;
    int selected_box_index;
    FadeAnimation* fade_out;
    bool is_2p;

public:
    BoxManager();
    void select_box();
    bool is_box_selected();
    bool is_finished();
    Screens selected_box();
    void move_left();
    void move_right();
    void update(double current_time_ms, bool is_2p);
    void draw();
};
