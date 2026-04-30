#pragma once
#include "../libs/screen.h"
#include "../libs/text.h"
#include "../libs/audio.h"
#include "../objects/enums.h"
#include "../objects/global/timer.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/indicator.h"
#include "../objects/song_select/file_navigator/box_dan.h"
#include "../objects/song_select/file_navigator/navigator.h"

class DanNavigator {
public:
    std::vector<std::unique_ptr<DanBox>> boxes;
    int selected_index = 0;

    void init(const std::vector<fs::path>& song_paths);
    void move_left();
    void move_right();
    void skip(int delta);
    DanBox* get_current();
    void update(double current_ms);
    void draw();

private:
    static constexpr float BOX_CENTER = 594.0f;
    static constexpr float BASE_SPACING = 150.0f;
    static constexpr float SIDE_OFFSET_L = 200.0f;
    static constexpr float SIDE_OFFSET_R = 500.0f;

    void set_positions(bool init, float duration);

    int total_notes_for(const std::vector<DanSongEntry>& songs);
};

class DanSelectScreen : public Screen {
public:
    DanSelectScreen() : Screen("dan_select") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;

private:
    DanNavigator dan_navigator;
    CoinOverlay coin_overlay;
    AllNetIcon allnet_indicator;
    std::unique_ptr<Indicator> indicator;
    SongSelectState state = SongSelectState::BROWSING;

    bool is_confirmed = false;
    FadeAnimation* confirm_fade = nullptr;

    double last_moved = 0;

    void handle_input_browsing(double current_ms);
    void handle_input_selected();

    void draw_confirm_overlay();
};
