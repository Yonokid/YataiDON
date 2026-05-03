#pragma once
#include "../libs/screen.h"
#include <memory>
#include <vector>
#include <string>

class SandboxScreen : public Screen {
public:
    struct Fixture {
        std::string name;
        virtual ~Fixture() = default;
        virtual void reset(double ms) = 0;
        virtual void update(double ms) = 0;
        virtual void draw() = 0;
        virtual uint32_t anchor_texture_id() = 0;  // texture whose skin position becomes the viewport center
        virtual void on_space(double ms) {}
        virtual void on_tab(double ms) {}
        virtual std::vector<std::string> debug_lines() { return {}; }
        virtual std::string controls() { return "SPACE/click: trigger  R/r-click: reset"; }
        virtual std::vector<std::string> type_names() { return {}; }
        virtual int  get_type() { return 0; }
        virtual void set_type(int, double) {}
    };

private:
    std::vector<std::unique_ptr<Fixture>> fixtures;
    int    fixture_idx = 0;
    double current_ms       = 0.0;
    double fixture_start_ms = 0.0;
    bool   paused           = false;

    static constexpr int    PANEL_W  = 220;
    static constexpr int    ITEM_H   = 32;
    static constexpr double FRAME_MS = 1000.0 / 60.0;

    void draw_panel() const;
    void draw_debug() const;
    std::optional<Screens> handle_input();

public:
    SandboxScreen() : Screen("sandbox") {}
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
