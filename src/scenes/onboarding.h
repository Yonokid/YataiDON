#pragma once

#include <string>
#include <vector>

#include "../libs/gfx/color_utils.h"
#include "../libs/screen.h"

class OnboardingScreen : public Screen {
private:
    std::vector<bool> selected;
    std::vector<bool> installed;
    bool online = false;
    std::string language;
    int row = 0;

    bool downloading = false;
    bool download_started = false;
    std::string error;
    bool skinless_start = false;

    int skin_count() const;
    int language_row() const { return skin_count(); }
    int online_row() const { return skin_count() + 1; }
    int continue_row() const { return skin_count() + 2; }
    void cycle_language();
    int required_by(int skin) const;
    void toggle_skin(int skin);
    void begin_download();
    bool apply();
    void handle_tap(Vec2 tap);
    void draw_line(const std::string& text, float y, int size, Color color);

public:
    OnboardingScreen() : Screen("onboarding") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
