#pragma once

#include "config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <raylib.h>

namespace fs = std::filesystem;

enum class PlayerNum {
    ALL = 0,
    P1 = 1,
    P2 = 2,
    TWO_PLAYER = 3,
    DAN = 4,
    AI = 5
};

namespace ScoreMethod {
    const std::string GEN3 = "gen3";
    const std::string SHINUCHI = "shinuchi";
}

enum class Difficulty {
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

struct Modifiers {
    bool auto_play = false;
    float speed = 1.0f;
    bool display = false;
    bool inverse = false;
    int random = 0;
    int subdiff = 0;
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
};

struct DanResultExam {
    float progress = 0.0f;
    int counter_value = 0;
    std::string bar_texture = "exam_red";
    bool failed = false;
};

struct DanResultData {
    int dan_color = 0;
    std::string dan_title = "default_title";
    int score = 0;
    float gauge_length = 0.0f;
    int max_combo = 0;
    std::vector<DanResultSong> songs;
    std::vector<void*> exams;  // Using void* for Any type
    std::vector<DanResultExam> exam_data;
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
    std::vector<std::tuple<void*, int, int, int>> selected_dan;  // Using void* for Any type
    std::vector<void*> selected_dan_exam;  // Using void* for Exam objects
    int dan_color = 0;
    int selected_difficulty = 0;
    std::string song_title = "default_title";
    int genre_index = 0;
    ResultData result_data;
    DanResultData dan_result_data;
};

struct CameraConfig {
    Vector2 offset = {0.0f, 0.0f};
    float zoom = 1.0f;
    float h_scale = 1.0f;
    float v_scale = 1.0f;
    float rotation = 0.0f;
    Color border_color = BLACK;
};

struct GlobalData {
    int songs_played = 0;
    CameraConfig camera;
    Font font = GetFontDefault();
    std::unordered_set<int> font_codepoints;
    Config* config = nullptr;  // Using pointer, initialize appropriately
    std::unordered_map<std::string, std::vector<std::unordered_map<std::string, std::string>>> song_hashes;  // Hash to metadata
    std::unordered_map<fs::path, std::string> song_paths;  // Path to hash
    std::string score_db;
    float song_progress = 0.0f;
    int total_songs = 0;
    std::vector<int> hit_sound = {0, 0, 0};
    PlayerNum player_num = PlayerNum::P1;
    int input_locked = 0;
    std::vector<Modifiers> modifiers = std::vector<Modifiers>(3);
    std::vector<SessionData> session_data = std::vector<SessionData>(3);

    GlobalData() {
        // Initialize vectors with default-constructed elements
        modifiers.resize(3);
        session_data.resize(3);
    }
};

extern GlobalData global_data;
