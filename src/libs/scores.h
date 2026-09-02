#pragma once

#include "global_data.h"
#include <chrono>
#include <sqlite3.h>
#include <mutex>

inline int64_t unix_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

struct PlayerData {
    int player_id            = 0;
    std::string username     = "";
    std::string title        = "";
    int title_bg             = 0;
    int dan                  = -1;      // -1 = no 段位, matches the column default
    bool gold                = false;
    bool rainbow             = false;
    bool modifier_auto       = false;
    int modifier_speed       = 10;      // 1.0x, in tenths
    bool modifier_display    = false;
    bool modifier_inverse    = false;
    int modifier_random      = 0;
    bool modifier_skip       = false;
    int neiro_index          = 0;
    ray::Color chara_color_1 = ray::Color{0x68, 0xBF, 0xC0, 0xFF};
    ray::Color chara_color_2 = ray::Color{0xF9, 0x47, 0x28, 0xFF};
    ray::Color chara_color_3 = ray::Color{0xF9, 0xF0, 0xE1, 0xFF};
    int chara_head_index     = 0;
    int chara_body_index     = 0;
    int chara_cos_index      = 0;
    bool chara_is_costume    = true;
    int chara_paint_index    = 0;
    int chara_face_index     = 0;
    int chara_acce_index     = 0;
};

struct DanRecord {
    int dan_index = -1;
    int rank      = 0;
    int score     = 0;
    int arrival   = 0;
};

struct Score {
    Crown crown;
    Rank rank;
    int score;
    int good;
    int ok;
    int bad;
    int drumroll;
    int max_combo;
};

class ScoresManager {
private:
    sqlite3* db_fsd;
    mutable std::mutex maps_mutex;
    std::unordered_map<fs::path, std::array<std::string, 5>> path_to_hashes;
    std::unordered_map<std::string, fs::path> single_hash_to_path;
    std::unordered_map<std::string, fs::path> diff_hash_to_path;
    std::map<std::tuple<std::string, int, int>, Score> score_cache;
    void load_score_cache();
public:
    int player_1;
    int player_2;
    PlayerData player_1_data;
    PlayerData player_2_data;
    ScoresManager(const fs::path& db_path);
    void py_taiko_import(const fs::path& old_db_path);
    void export_to_hiroba(const std::string& access_code, int player_id);
    int sync_from_server(const std::string& access_code);
    std::optional<Score> get_score(std::string& hash, int difficulty, int player_id);
    Score save_score(std::string& hash, int difficulty, int player_id, Score score, int64_t played_at, const std::string& modifiers_json);
    void add_path_binding(const fs::path& path, const std::array<std::string, 5>& hashes);
    std::array<std::string, 5> get_hashes(const fs::path& path);
    std::string get_single_hash(const fs::path& path);
    std::optional<fs::path> get_path_by_hash(const std::string& single_hash);
    std::optional<fs::path> get_path_by_diff_hash(const std::string& diff_hash);
    void add_song(const std::array<std::string, 5>& hash, const std::string& title, const std::string& subtitle);
    void remap_hashes(const std::unordered_map<std::string, std::string>& old_to_new);
    std::optional<DanRecord> get_dan_record(int player_id, const std::string& course_title);
    void save_dan_record(int player_id, const std::string& course_title, const DanRecord& rec);
    std::optional<PlayerData> get_player_data(int player_id);
    void save_player_data(const PlayerData& player);
    int add_player(const std::string& name);
    void begin_transaction();
    void commit();
};

extern ScoresManager* _scores_manager_ptr;
#define scores_manager (*_scores_manager_ptr)
void init_scores_manager(bool gen3);

inline ray::Color chara_default_color_1(int player_id) {
    return (player_id % 2 == 0) ? ray::Color{249, 71, 40, 255} : ray::Color{104, 191, 192, 255};
}
inline ray::Color chara_default_color_2(int player_id) {
    return (player_id % 2 == 0) ? ray::Color{104, 191, 192, 255} : ray::Color{249, 71, 40, 255};
}

inline Modifiers player_data_to_modifiers(const PlayerData& pd) {
    Modifiers m;
    m.auto_play = pd.modifier_auto;
    m.speed     = pd.modifier_speed;
    m.display   = pd.modifier_display;
    m.inverse   = pd.modifier_inverse;
    m.random    = pd.modifier_random;
    m.skip      = pd.modifier_skip;
    return m;
}
