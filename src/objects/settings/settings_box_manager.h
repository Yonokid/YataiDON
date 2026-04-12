#pragma once
#include "settings_box.h"
#include <rapidjson/document.h>
#include <vector>
#include <string>

class SettingsBoxManager {
private:
    std::vector<SettingsBox*> boxes;
    int   num_boxes;
    int   selected_box_index;
    bool  box_selected;

public:
    explicit SettingsBoxManager(const rapidjson::Document& tmpl);
    ~SettingsBoxManager();

    // Returns true if the exit box was selected, otherwise false
    bool select_box();

    void move_left();
    void move_right();
    void update(double current_time_ms);
    void draw();
};
