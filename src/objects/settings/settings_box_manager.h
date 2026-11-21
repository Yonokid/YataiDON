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

    struct Drag {
        Vec2 start;
        float scrolled_y;
        double moved_ms;
        float velocity{0.0f}; // px per ms
        int value_steps{0};
        bool from_list;
        bool moved{false};
    };
    std::optional<Drag> drag;
    float fling_velocity{0.0f};
    bool touch_mode{false}; // hides the list highlight until a key or drum is used
    double last_update_ms{0.0};

    bool tap(Vec2 world);
    bool scroll_list(float dy);

public:
    explicit SettingsBoxManager(const rapidjson::Document& tmpl);
    ~SettingsBoxManager();

    // Returns true if the exit box was selected, otherwise false
    bool select_box();
    bool editing_text() const { return box_selected && boxes[selected_box_index]->editing_text(); }
    bool editing() const { return box_selected && boxes[selected_box_index]->editing(); }

    std::optional<Screens> pending_screen_change();

    // World-space finger state for one frame; returns true if the exit box was tapped.
    bool touch(Vec2 start, Vec2 pos, bool down);
    void use_keys() { touch_mode = false; }

    void move_left();
    void move_right();
    void update(double current_time_ms);
    void draw();
};
