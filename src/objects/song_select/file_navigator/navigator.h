#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <unordered_map>

#include "../../../libs/song_library.h"
#include "box_song.h"
#include "genre_bg.h"
#include "navigation_load.h"
#include "navigation_scan.h"

class SongSelectScript;

struct CourseStats {
    int total = 0;
    int full_combos = 0;
    int clears = 0;
};

struct InlineState {
    std::unique_ptr<FolderBox> saved_folder_box;
    int folder_index;
    int first_song_index;
    int songs_count;
    bool fading_out = false;
};

using Statistics = std::map<int, std::map<int, CourseStats>>;

class Navigator {
private:
    std::vector<fs::path> root_paths;
    std::vector<std::unique_ptr<BaseBox>> items;
    TextureObject* t_background = nullptr;
    SongLibrary& song_library;
    int open_index;
    bool is_init = false;
    bool is_preloaded = false;
    bool reloading_roots = false;
    bool built_hide_dan = false;

    std::optional<InlineState> inline_state;
    std::optional<fs::path> pending_inline_path;
    FolderBox* pending_inline_folder = nullptr;
    BoxDef inline_back_def{}; // genre/colours for the repeated back boxes of the open folder
    BoxDef pending_inline_box_def{};
    bool is_inline = false;

    std::optional<GenreBG> genre_bg;
    int genre_bg_start;
    int genre_bg_end;
    std::optional<float> genre_bg_end_pos;
    GenreIndex bg_genre_index;
    GenreIndex last_bg_genre_index;
    bool bg_genre_pending = true;

    FadeAnimation* background_fade_change;
    MoveAnimation* background_move;

    NavigationLoader loader;
    uint64_t request_id = 0;
    NavigationResults load_results;
    bool loading_complete = false;
    bool synchronous_load = false;
    bool summary_loading = false;
    std::optional<std::pair<LoadRequest::Kind, fs::path>> active_directory_load;
    std::optional<std::pair<LoadRequest::Kind, fs::path>> interrupted_directory_load;
    bool resume_directory_load();
    uint64_t score_revision = 0;
    uint64_t song_list_revision = 0;
    std::string summary_player_1;
    std::string summary_player_2;
    std::map<fs::path, FolderSummary> folder_summaries;
    std::set<fs::path> failed_folder_summaries;
    std::vector<fs::path> active_summary_paths;
    uint64_t active_summary_revision = 0;
    void invalidate_folder_summaries();
    void request_folder_summaries();

    LoadRequest make_load_request(LoadRequest::Kind kind, const fs::path& path = {});
    void submit_load(LoadRequest request);
    void apply_entry(PreparedEntry prepared, bool inline_mode);

    std::optional<fs::path> reopen_folder_path;
    std::optional<fs::path> reopen_song_path;
    std::optional<fs::path> restore_cursor_path;

    std::optional<fs::path> recent_folder_path;
    std::optional<fs::path> favorite_folder_path;
    std::set<std::string> favorite_songs;
    navigation_scan::FolderScanner folder_scanner;
    std::unordered_map<std::string, BoxDef> box_def_cache;

    bool awaiting_diff_sort = false;
    std::optional<std::array<int, 3>> diff_sort_filter;
    std::optional<std::pair<int, int>> last_diff_sort_result;

    bool vertical_gallery = false;

    void navigate(int delta, bool snap);
    void set_positions(bool init, float duration);

    void setup_back_box(const fs::path& path, bool has_children, const BaseBox* from = nullptr);

    void request_directory(const fs::path& path);
    void load_all_roots();
    void request_inline(const fs::path& path, BoxDef box_def);
    void promote_recent_box(const SongBox* song);
    void collapse_inline_now();
    void flush_pending_boxes();
    void exit_inline();
    void begin_inline_load();

public:
    Navigator();
    ~Navigator();

    std::atomic<bool> is_processing{false};
    bool hide_dan = false;
    bool is_2p = false;
    bool inline_streaming = false;
    fs::path current_path;
    std::string current_search;

    SongSelectScript* script = nullptr;

    void cancel_loading();
    void preload(std::vector<fs::path> songs_paths);
    void init(std::vector<fs::path> songs_paths);

    void reset_for_skin_reload();
    void add_to_recent(const SongBox* song);
    void toggle_favorite(SongBox* song);
    void refresh_scores();
    BoxDef parse_box_def(const fs::path& path);
    static BoxDef parse_box_def_uncached(const fs::path& path);
    bool needs_diff_sort() const { return awaiting_diff_sort; }
    bool diff_sort_ready() { return awaiting_diff_sort; }
    void apply_diff_sort(int course, int level, int order = 1);
    void cancel_diff_sort();
    void load_current_directory(fs::path path);
    bool jump_to_song(const std::string& hash);
    bool jump_to_song_path(const fs::path& song_path);
    void enter_diff_select();
    void exit_diff_select();
    float get_diff_fade_in();
    bool is_directory(BaseBox* item);
    bool is_song(BaseBox* item);
    BaseBox* get_current_item();
    Statistics get_statistics(const fs::path& path, const std::stop_token& stop);

    void move_left();
    void move_right();
    void skip_left();
    void skip_right();

    void update(double current_ms);
    void draw_background();
    void draw();
    void draw_diff_select_bg();
    void draw_score_history();

    MoveAnimation* background_move_anim() const { return background_move; }
    FadeAnimation* background_fade_anim() const { return background_fade_change; }
    FolderBox* lua_current_folder() const {
        if (!inline_state.has_value()) {
            return nullptr;
        }
        return inline_state->saved_folder_box.get();
    }
    int bg_genre_frame() const { return genre_to_ref_frame(bg_genre_index); }
    int last_bg_genre_frame() const { return genre_to_ref_frame(last_bg_genre_index); }

    enum WheelEvent {
        WHEEL_EVENT_NONE = 0,
        WHEEL_EVENT_SCENE_ENTRY = 1,
        WHEEL_EVENT_CURSOR_MOVE = 2,
        WHEEL_EVENT_OPEN_BEGIN = 3,
        WHEEL_EVENT_OPEN_SWAP = 4,
        WHEEL_EVENT_CLOSE_BEGIN = 5,
        WHEEL_EVENT_CLOSE_SWAP = 6,
        WHEEL_EVENT_COURSE_BACK = 7,
    };
    int wheel_event = WHEEL_EVENT_NONE;
    int wheel_event_seq = 0;
    double wheel_leg_ms = 0.0;
    bool wheel_events_read = false; // the skin script reads the wheel events
    void emit_wheel_event(int id);

    static constexpr double kSwapDelayMs = (114.0 - 45.0) / 60.0 * 1000.0;
    bool swap_is_early(int begin_id) const;
};

extern Navigator navigator;
