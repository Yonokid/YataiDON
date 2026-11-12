#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "settings_box.h"

class SettingsBoxManager {
private:
    std::vector<std::unique_ptr<SettingsBox>> boxes;
    static constexpr int INITIAL_SELECTED = 3;
    int selected_box_index{INITIAL_SELECTED};
    bool box_selected{false};

public:
    explicit SettingsBoxManager(const rapidjson::Document& tmpl);
    ~SettingsBoxManager();

    // Returns true if the exit box was selected, otherwise false
    bool select_box();
    bool editing_text() const { return box_selected && boxes[selected_box_index]->editing_text(); }
    bool editing() const { return box_selected && boxes[selected_box_index]->editing(); }

    std::optional<Screens> pending_screen_change();

    // world-space tap; returns true if the exit box was selected
    bool tap(Vec2 world);

    void move_left();
    void move_right();
    void update(double current_time_ms);
    void draw();
};
