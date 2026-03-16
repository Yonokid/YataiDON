#pragma once

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../libs/parsers/tja.h"
#include "../libs/scores.h"

class LoadingScreen : public Screen {
private:
    std::atomic<bool> loading_complete{false};
    std::atomic<float> progress{0.0f};
    std::vector<fs::path> songs;

    float progress_bar_width;
    float progress_bar_height;
    float progress_bar_x;
    float progress_bar_y;

    std::thread loading_thread;

    FadeAnimation* fade_in;
    AllNetIcon allnet_indicator;

    void load_song_hashes();

    void load_navigator();

public:
    LoadingScreen() : Screen("loading") {
    }

    void on_screen_start() override;

    Screens on_screen_end(Screens next_screen) override;

    std::optional<Screens> update() override;

    void draw() override;
};
