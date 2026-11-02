#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"

class LoadingScreen : public Screen {
private:
    std::atomic<bool> loading_complete{false};
    std::atomic<float> progress{0.0f};
    std::vector<fs::path> songs;

    Rect progress_bar;

    TextureObject* t_warning = nullptr;

    // Screen option loading_countdown: the skin's loading/loading_countdown module gives `seconds`, how
    // long the screen stays at least, and draw(left_ms), which paints the countdown.
    bool has_countdown = false;
    double countdown_ms = 0.0;
    double start_ms = 0.0;
    // a don (1P / 2P: F J / X C by default) skips the rest of the countdown; the song scan still
    // has to finish, so a press during it is kept until then
    bool skip_requested = false;

    std::thread loading_thread;

    std::unique_ptr<FadeAnimation> fade_in;
    std::optional<AllNetIcon> allnet_indicator;

    bool skin_reloaded = false;

    void init_visuals();

    void load_song_hashes(bool populate_library);

    void load_navigator();

public:
    LoadingScreen() : Screen("loading") {}

    ~LoadingScreen() override {
        if (loading_thread.joinable()) {
            loading_thread.join();
        }
    }

    void on_screen_start() override;

    Screens on_screen_end(Screens next_screen) override;

    std::optional<Screens> update() override;

    void draw() override;
};
