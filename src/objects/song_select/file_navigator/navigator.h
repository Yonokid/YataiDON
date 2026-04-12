#pragma once
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include "box_base.h"
#include "box_folder.h"
#include "box_back.h"
#include "box_song.h"
#include "genre_bg.h"

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

    void set_positions(bool init, float duration);
    bool is_song_file(const fs::path& path);
    bool has_def_file(const std::filesystem::path& path);
    int  get_tja_count(const std::filesystem::path& path);
    BoxDef parse_box_def(const fs::path& path);
    void setup_back_box(const fs::path& path, bool has_children);
    bool has_child_folders(const fs::path& path);

    void enqueue_box(std::unique_ptr<BaseBox> box);
    void enqueue_inline_box(std::unique_ptr<BaseBox> box);
    void parse_song_list(const fs::path& path, BoxDef box_def, bool inline_mode);
    void load_current_directory_async(const fs::path path);
    void load_songs_inline_async(const fs::path path, BoxDef box_def);
    void join_loader();
    void flush_pending_boxes();
    void exit_inline();

public:
    Navigator();
    ~Navigator();

    bool is_processing = false;
    fs::path current_path;

    void init(std::vector<fs::path> songs_paths);
    void load_current_directory(const fs::path path);
    void enter_diff_select();
    void exit_diff_select();
    bool is_directory(BaseBox* item);
    bool is_song(BaseBox* item);
    BaseBox* get_current_item();

    void move_left();
    void move_right();
    void skip_left();
    void skip_right();

    void update(double current_ms);
    void draw(bool is_ura);
};

extern Navigator navigator;
