#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sol/sol.hpp>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../../libs/parsers/tja.h"
#include "../song_select/file_navigator/box_dan.h"

struct DanBoxData {
    fs::path json_path;
    std::string title;
    int color = 0;
    int rank = -1;
    int dan_index = -1;
    bool gaiden = false;
    bool gen3 = false; // "gen3": true forces gen3 scoring for this course
    std::vector<DanSongEntry> songs;
    std::vector<Exam> exams;
    int total_notes = 0;
    std::vector<std::pair<std::string, std::string>> song_titles;
};

class DanNavigator {
public:
    // the skin's ribbon: box centre, spacing between boxes, and the extra gap left / right of the selection
    void set_ribbon(float center, float spacing, float side_l, float side_r) {
        ribbon.legacy = false;
        if (center != 0) {
            ribbon.center = center;
        }
        if (spacing != 0) {
            ribbon.spacing = spacing;
        }
        ribbon.side_l = side_l;
        ribbon.side_r = side_r;
    }
    std::vector<std::unique_ptr<DanBox>> boxes;
    int selected_index = 0;

    bool paint_tried = false;
    bool paint_ok = false;
    sol::table lua_paint;
    sol::protected_function fn_draw_cursor;
    sol::protected_function fn_draw_box;
    sol::protected_function fn_update;
    void load_paint_surface();

    double last_moved = 0;

    TextureObject* t_cursor = nullptr;
    TextureObject* t_arrow_r = nullptr;
    TextureObject* t_arrow_l = nullptr;

    void init(const std::vector<fs::path>& song_paths);

    void begin_init(const std::vector<fs::path>& song_paths);
    bool poll_init();
    bool init_running() const { return scan_thread.joinable() && !scan_done.load(); }
    void abort_init();
    ~DanNavigator();
    int scan_root(const fs::path& root_path);
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
        bool legacy = true;
        float center = BOX_CENTER;
        float spacing = BASE_SPACING;
        float side_l = SIDE_OFFSET_L;
        float side_r = SIDE_OFFSET_R;
    };
    RibbonLayout ribbon; // legacy defaults until the skin gives its own (set_ribbon)
    RibbonLayout ribbon_layout() const { return ribbon; }

    void set_positions(bool init, float duration);

    int total_notes_for(const std::vector<DanSongEntry>& songs);

    // Chart reads shared by the scan's worker threads: a song sits in many courses, and every
    // course used to re-read it twice (metadata, then notes)
    struct ScanMemo {
        std::shared_ptr<const class SongIndex> index;
        std::mutex mutex;
        std::map<fs::path, std::shared_ptr<const TJAMetadata>> metadata;
        std::map<std::pair<fs::path, int>, int> note_counts;
        std::map<fs::path, int> genres; // by box.def folder
    };
    ScanMemo* memo_ = nullptr; // set while a scan runs; nullptr = read every time
    std::shared_ptr<const TJAMetadata> song_metadata(const fs::path& path);
    int song_note_count(const fs::path& path, int difficulty);
    int box_genre(const fs::path& box_def_dir);
    void collect_dan_jsons(const fs::path& root_path, std::vector<fs::path>& out);
    std::vector<DanBoxData> load_dan_jsons(const std::vector<fs::path>& jsons);
    std::vector<DanBoxData> scan_roots(const std::vector<fs::path>& roots);

    Exam parse_exam(const rapidjson::Value& e);

    std::optional<DanSongEntry> load_song_entry(const rapidjson::Value& chart,
                                                std::pair<std::string, std::string>* titles_out = nullptr);

    std::optional<DanBoxData> load_dan_box_data(const fs::path& json_path);
    std::unique_ptr<DanBox> load_dan_box(const fs::path& json_path);
    static std::unique_ptr<DanBox> make_box(const DanBoxData& d);
    std::vector<DanBoxData> scan_all_data(const std::vector<fs::path>& song_paths);
    int scan_root_data(const fs::path& root_path, std::vector<DanBoxData>& out);
    void publish(std::vector<DanBoxData>&& data);

    std::thread scan_thread;
    std::atomic<bool> scan_done{false};
    std::atomic<bool> scan_abort{false};
    std::atomic<bool> scan_published{false};
    fs::path restore_selection_path;
    std::mutex scan_mutex;
    std::vector<DanBoxData> scan_result; // guarded by scan_mutex
};
