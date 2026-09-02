#pragma once

#include <cmath>

#include "config.h"
#include "ray.h"
#include "parsers/tja.h"

namespace fs = std::filesystem;

namespace ScoreMethod {
    const std::string GEN3 = "gen3";
    const std::string SHINUCHI = "shinuchi";
}

enum class Difficulty {
    BACK = -3,
    MODIFIER = -2,
    NEIRO = -1,
    EASY = 0,
    NORMAL = 1,
    HARD = 2,
    ONI = 3,
    URA = 4,
    TOWER = 5,
    DAN = 6
};

enum class Crown {
    NONE = 0,
    CLEAR = 1,
    FC = 2,
    DFC = 3
};

//underscore to avoid conflict with raylib colors
enum class Rank {
    _NONE = 0,
    _WHITE = 1,
    _BRONZE = 2,
    _SILVER = 3,
    _GOLD = 4,
    _PINK = 5,
    _PURPLE = 6,
    _RAINBOW = 7
};

struct Exam {
    std::string type;   // "gauge","combo","judgebad","judgegood","judgeperfect","hit","score"
    int red = 0;
    int gold = 0;
    std::string range;  // "less" or "more"
    bool gothrough = true;
};

inline std::string dan_bar_state(const Exam& exam, int value,
                                 bool live = false,
                                 bool near_end = false,
                                 bool just_before_end = false) {
    const bool down = (exam.range == "less");
    // CheckMax
    if (exam.gold > 0 && ((down && value < exam.gold) || (!down && value >= exam.gold))) {
        if (!live || !down) return "max";
        if (just_before_end) return "max_soon2";
        if (near_end)        return "max_soon";
        // fall through to the flat palette -- no `max` during play
    }
    int gauge = (exam.red > 0)
        ? (int)std::floor(100.0 * (double)value / (double)exam.red) : 100;
    if (gauge > 100) gauge = 100;
    if (down) { gauge = 100 - gauge; if (gauge < 0) gauge = 0; }
    if (gauge <= 0) return "empty";
    if (!down) {
        if (gauge <= 49) return "up_50";
        if (gauge <= 99) return "up_80";
        return "up_100";
    }
    const int good = (exam.red > 0 && exam.gold > 0)
        ? 100 - (int)std::floor(100.0 * (double)exam.gold / (double)exam.red) : 100;
    if (gauge < 30)    return "down_80";
    if (gauge <= good) return "up_80";
    return "down_100";
}

struct DanSongEntry {
    fs::path song_path;
    int genre_index = 0;
    int difficulty = 0;
    int level = 0;
    bool hidden = false;
};

struct DanResultSong {
    int selected_difficulty = 0;
    int diff_level = 0;
    std::string song_title = "default_title";
    int genre_index = 0;
    int good = 0;
    int ok = 0;
    int bad = 0;
    int drumroll = 0;
    bool hidden = false;
    bool unreached = false;
};

struct DanResultExam {
    float progress = 0.0f;
    int counter_value = 0;
    std::string bar_texture = "exam_red";
    bool failed = false;
    int   song_value[3]    = {0, 0, 0};
    float song_progress[3] = {0.0f, 0.0f, 0.0f};
    int   song_count       = 0;
    int tier = 0;
    std::string bar_state = "empty";
    std::string song_state[3] = {"empty", "empty", "empty"};
};

struct DanResultData {
    int dan_color = 0;
    int dan_rank = -1;
    int dan_index = -1;
    int dan_index_max = -1;
    bool is_gaiden = false;
    std::string dan_title = "default_title";
    int score = 0;
    float gauge_length = 0.0f;
    int max_combo = 0;
    std::vector<DanResultSong> songs;
    std::vector<Exam> exams;
    std::vector<DanResultExam> exam_data;
    int odai_result = -1;
};

struct ResultData {
    int score = 0;
    int good = 0;
    int ok = 0;
    int bad = 0;
    int max_combo = 0;
    int total_drumroll = 0;
    float gauge_length = 0.0f;
    int prev_score = 0;
};

struct SessionData {
    fs::path selected_song;
    std::string song_hash;
    fs::path selected_dan_folder;
    std::vector<DanSongEntry> selected_dan;
    std::vector<Exam> selected_dan_exam;
    int dan_color = 0;
    int dan_rank = -1;
    int dan_index = -1;
    int dan_index_max = -1;
    bool dan_gaiden = false;
    int selected_difficulty = 0;
    std::string song_title = "default_title";
    std::string song_subtitle = "default_subtitle";
    bool song_subtitle_full_display = false;
    int genre_index = 0;
    ResultData result_data;
    DanResultData dan_result_data;
};

struct CameraConfig {
    ray::Vector2 offset = {0.0f, 0.0f};
    float zoom = 1.0f;
    float h_scale = 1.0f;
    float v_scale = 1.0f;
    float rotation = 0.0f;
    ray::Color border_color = ray::BLACK;
};

struct GlobalData {
    int songs_played = 0;
    std::string current_screen = "LOADING";
    std::string previous_screen;
    bool in_transition = false;
    std::string title_state = "";
    double      title_state_start_ms = 0.0;
    int  live_combo = 0;
    int  live_score = 0;
    int  live_drumroll = 0;
    bool live_gogo = false;
    double live_soul = 0.0;
    bool live_is_clear = false;
    bool live_is_rainbow = false;
    int  live_skip_count = -1;
    bool live_skip_used = false;
    bool force_auto_play = false;
    bool returned_from_result = false;
    bool entry_join_pending = false;
    PlayerNum entry_joined_seat = PlayerNum::P1;
    CameraConfig camera;
    Config* config = nullptr;  // Using pointer, initialize appropriately
    int total_songs = 0;
    PlayerNum player_num = PlayerNum::P1;
    PlayerNum first_login_player = PlayerNum::P1;
    int input_locked = 0;
    std::vector<SessionData> session_data = std::vector<SessionData>(3);
    std::vector<int> last_difficulty = std::vector<int>(3, -1);
    std::vector<fs::path> dan_folder = std::vector<fs::path>(3);

    GlobalData() {
        // Initialize vectors with default-constructed elements
        session_data.resize(3);
    }
};

void reset_session();
int get_player_id(PlayerNum player_num);

extern GlobalData global_data;
