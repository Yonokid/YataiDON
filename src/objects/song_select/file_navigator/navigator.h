#pragma once
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include "box_base.h"
#include "box_folder.h"
#include "box_back.h"
#include "box_song.h"
#include "genre_bg.h"
#include <chrono>

namespace ch = std::chrono;

struct CourseStats {
    int total       = 0;
    int full_combos = 0;
    int clears      = 0;
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
    std::map<std::pair<std::string, std::string>, fs::path> song_files;
    int open_index;
    bool is_init = false;

    std::optional<InlineState>  inline_state;
    std::optional<fs::path>     pending_inline_path;
    FolderBox*                  pending_inline_folder = nullptr;
    BoxDef                      pending_inline_box_def;
    bool is_inline = false;

    std::optional<GenreBG> genre_bg;
    int        genre_bg_start;
    int        genre_bg_end;
    std::optional<float> genre_bg_end_pos;
    GenreIndex bg_genre_index;
    GenreIndex last_bg_genre_index;

    FadeAnimation* background_fade_change;
    MoveAnimation* background_move;

    std::thread              loader_thread;
    std::mutex               pending_mutex;
    std::queue<std::unique_ptr<BaseBox>> pending_boxes;
    std::queue<std::unique_ptr<BaseBox>> pending_inline_boxes;
    std::atomic<bool>        loading_complete{false};
    std::atomic<bool>        abort_loading{false};

    std::optional<fs::path>  recent_folder_path;
    std::optional<fs::path>  favorite_folder_path;
    std::set<std::string>    favorite_songs;

    bool awaiting_diff_sort = false;
    std::optional<std::pair<int,int>> diff_sort_filter;
    std::optional<std::pair<int,int>> last_diff_sort_result;

    void set_positions(bool init, float duration);
    bool is_song_file(const fs::path& path);
    bool has_def_file(const std::filesystem::path& path);
    int  get_tja_count(const std::filesystem::path& path);
    BoxDef parse_box_def(const fs::path& path);
    fs::path find_box_def_folder(const fs::path& song_path);
    void setup_back_box(const fs::path& path, bool has_children);
    bool has_child_folders(const fs::path& path);

    void enqueue_box(std::unique_ptr<BaseBox> box);
    void enqueue_inline_box(std::unique_ptr<BaseBox> box);
    void parse_song_list(const fs::path& path, BoxDef box_def, bool inline_mode);
    void load_current_directory_async(const fs::path path);
    void load_collection_difficulty(const fs::path& path, const BoxDef& box_def, int course, int level);
    void load_collection_favorite(const fs::path& path, const BoxDef& box_def);
    void load_collection_new(const fs::path& path, const BoxDef& box_def);
    void load_collection_recent(const fs::path& path, const BoxDef& box_def);
    void load_collection_recommended(const fs::path& path, const BoxDef& box_def);
    void load_songs_inline_async(const fs::path path, BoxDef box_def);
    void join_loader();
    void flush_pending_boxes();
    void exit_inline();
    void begin_inline_load();

public:
    Navigator();
    ~Navigator();

    bool is_processing = false;
    fs::path current_path;

    void init(std::vector<fs::path> songs_paths);
    void add_to_recent(const SongBox* song);
    void toggle_favorite(SongBox* song);
    void refresh_scores();
    bool needs_diff_sort() const { return awaiting_diff_sort; }
    bool diff_sort_ready() { return awaiting_diff_sort; }
    void apply_diff_sort(int course, int level);
    void cancel_diff_sort();
    void load_current_directory(const fs::path path);
    void enter_diff_select();
    void exit_diff_select();
    bool is_directory(BaseBox* item);
    bool is_song(BaseBox* item);
    BaseBox* get_current_item();
    Statistics get_statistics(const fs::path& path);

    void move_left();
    void move_right();
    void skip_left();
    void skip_right();

    void update(double current_ms);
    void draw(bool is_ura);
    void draw_score_history();
};

extern Navigator navigator;
