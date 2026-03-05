#pragma once

#include <map>
#include "box_base.h"
#include "box_folder.h"
#include "box_back.h"
#include "box_song.h"

struct CourseStats {
    int total   = 0;
    int full_combos  = 0;
    int clears       = 0;
};

struct BoxDef {
    std::string name;
    TextureIndex texture_index;
    GenreIndex genre_index;
    std::string collection;
    std::optional<ray::Color> back_color;
    std::optional<ray::Color> fore_color;
};

using Statistics = std::map<int, std::map<int, CourseStats>>;

class Navigator {
private:
    std::vector<std::unique_ptr<BaseBox>> items;

    int open_index;

    bool is_init = false;

    void set_positions(bool init, float duration);
    bool is_song_file(const fs::path& path);
    bool has_subdirectories(const std::filesystem::path& path);
    int get_tja_count(const std::filesystem::path& path);
    BoxDef parse_box_def(const fs::path& path);

public:
    Navigator();

    void init(std::vector<fs::path> songs_paths);

    void load_current_directory(const fs::path path, bool clear_items);

    void enter_diff_select();

    void exit_diff_select();

    bool is_directory(BaseBox* item);

    bool is_song(BaseBox* item);

    BaseBox* get_current_item();

    void move_left();
    void move_right();

    void update(double current_ms);

    void draw();
};

extern Navigator navigator;
