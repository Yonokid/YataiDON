#pragma once

#include <map>
#include "box_base.h"
#include "box_folder.h"
#include "box_back.h"
#include "box_song.h"
#include "genre_bg.h"

struct CourseStats {
    int total   = 0;
    int full_combos  = 0;
    int clears       = 0;
};

struct InlineState {
    std::unique_ptr<FolderBox> saved_folder_box;
    int folder_index;           // where in items the FolderBox was
    int first_song_index;       // first inserted song/back position
    int songs_count;            // how many items were inserted
    bool fading_out = false;
};

using Statistics = std::map<int, std::map<int, CourseStats>>;

class Navigator {
private:
    std::vector<std::unique_ptr<BaseBox>> items;

    int open_index;

    bool is_init = false;
    std::optional<InlineState>  inline_state;
    std::optional<fs::path>     pending_inline_path;
    FolderBox* pending_inline_folder = nullptr;
    BoxDef                      pending_inline_box_def;
    bool is_inline = false;
    std::optional<GenreBG> genre_bg;
    int genre_bg_start;
    int genre_bg_end;

    GenreIndex bg_genre_index;
    GenreIndex last_bg_genre_index;

    FadeAnimation* background_fade_change;
    MoveAnimation* background_move;

    void set_positions(bool init, float duration);
    bool is_song_file(const fs::path& path);
    bool has_def_file(const std::filesystem::path& path);
    int get_tja_count(const std::filesystem::path& path);
    BoxDef parse_box_def(const fs::path& path);
    void setup_back_box(const fs::path& path, bool has_children);
    bool has_child_folders(const fs::path& path);
    void load_songs_inline(const fs::path& path, const BoxDef& box_def);
    void exit_inline();

public:
    Navigator();

    bool is_processing = false;
    void init(std::vector<fs::path> songs_paths);

    void load_current_directory(const fs::path path);

    void enter_diff_select();

    void exit_diff_select();

    bool is_directory(BaseBox* item);

    bool is_song(BaseBox* item);

    BaseBox* get_current_item();

    void move_left();
    void move_right();

    void update(double current_ms);

    void draw(bool is_ura);
};

extern Navigator navigator;
