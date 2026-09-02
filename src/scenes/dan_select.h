#pragma once

#include "../libs/screen.h"
#include "../objects/song_select/file_navigator/box_dan.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/indicator.h"
#include "../objects/song_select/modifier.h"
#include "../objects/global/timer.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/nameplate.h"
#include <sol/sol.hpp>
#include <atomic>
#include <thread>
#include <mutex>

struct DanBoxData {
    fs::path                   json_path;
    std::string                title;
    int                        color = 0;
    int                        rank = -1;
    int                        dan_index = -1;
    bool                       gaiden = false;
    std::vector<DanSongEntry>  songs;
    std::vector<Exam>          exams;
    int                        total_notes = 0;
    std::vector<std::pair<std::string, std::string>> song_titles;
};

class DanNavigator {
public:
    std::vector<std::unique_ptr<DanBox>> boxes;
    int selected_index = 0;

    bool paint_tried = false;
    bool paint_ok    = false;
    sol::table lua_paint;
    sol::protected_function fn_draw_cursor;
    void load_paint_surface();

    double last_moved = 0;

    void init(const std::vector<fs::path>& song_paths);

    void begin_init(const std::vector<fs::path>& song_paths);
    bool poll_init();
    bool init_running() const { return scan_thread.joinable() && !scan_done.load(); }
    void abort_init();
    ~DanNavigator();
    int  scan_root(const fs::path& root_path);
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

    struct RibbonLayout {
        bool  legacy  = true;
        float center  = BOX_CENTER;
        float spacing = BASE_SPACING;
        float side_l  = SIDE_OFFSET_L;
        float side_r  = SIDE_OFFSET_R;
    };
    RibbonLayout ribbon_layout() const;

    void set_positions(bool init, float duration);

    int total_notes_for(const std::vector<DanSongEntry>& songs);

    Exam parse_exam(const rapidjson::Value& e);

    std::optional<DanSongEntry> load_song_entry(const rapidjson::Value& chart,
                                                std::pair<std::string, std::string>* titles_out = nullptr);

    std::optional<DanBoxData> load_dan_box_data(const fs::path& json_path);
    std::unique_ptr<DanBox>   load_dan_box(const fs::path& json_path);
    static std::unique_ptr<DanBox> make_box(const DanBoxData& d);
    std::vector<DanBoxData> scan_all_data(const std::vector<fs::path>& song_paths);
    int scan_root_data(const fs::path& root_path, std::vector<DanBoxData>& out);
    void publish(std::vector<DanBoxData>&& data);

    std::thread              scan_thread;
    std::atomic<bool>        scan_done{false};
    std::atomic<bool>        scan_abort{false};
    bool                     scan_published = false;
    std::mutex               scan_mutex;
    std::vector<DanBoxData>  scan_result;   // guarded by scan_mutex
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

    std::unique_ptr<Chara3D> chara;
    Nameplate nameplate;

    enum ConfirmEntry { CONFIRM_OPTION = 0, CONFIRM_YES = 1, CONFIRM_NO = 2 };
    int confirm_index = CONFIRM_NO;
    FadeAnimation* confirm_fade = nullptr;

    double confirm_opened_at = 0;

    PlayerData dan_player_data;
    std::optional<ModifierSelector> modifier_selector;

    double last_moved = 0;

    bool wheel_locked = false;
    double wheel_tick_epoch = 0;
    long long wheel_tick_seen = -1;

    std::unique_ptr<Timer> select_timer;
    bool   timer_started   = false;
    bool   timer_fired     = false;
    double screen_start_ms = 0;
    double intro_ms        = 0;

    static constexpr double SCAN_TIMEOUT_MS = 20000.0;
    bool   scan_ready      = false;
    double scan_ready_ms   = 0;
    double scan_begin_ms   = 0;
    bool   legacy_blocking = false;

    void   publish_scan_state(double current_ms);

    std::optional<Screens> tick_timer(double current_ms);
    void open_confirm(double current_ms);

    void handle_input_browsing(double current_ms);
    std::optional<Screens> handle_input_selected();

    void draw_confirm_overlay();
};
