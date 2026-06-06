#pragma once

#include "global_data.h"
#include <sqlite3.h>

struct PlayerData {
    int player_id;
    std::string username;
    std::string title;
    int title_bg;
    int dan;
    bool gold;
    bool rainbow;
    bool modifier_auto;
    int modifier_speed;
    bool modifier_display;
    bool modifier_inverse;
    int modifier_random;
    int neiro_index;
    ray::Color chara_color_1;
    ray::Color chara_color_2;
    ray::Color chara_color_3;
    int chara_head_index;
    int chara_body_index;
    int chara_cos_index;
    bool chara_is_costume;
    int chara_paint_index;
    int chara_face_index;
    int chara_acce_index;
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
    std::unordered_map<fs::path, std::array<std::string, 5>> path_to_hashes;
    std::unordered_map<std::string, fs::path> single_hash_to_path;
    std::map<std::tuple<std::string, int, int>, Score> score_cache;
    void load_score_cache();
public:
    int player_1;
    int player_2;
    PlayerData player_1_data;
    PlayerData player_2_data;
    ScoresManager(const fs::path& db_path);
    void py_taiko_import(const fs::path& old_db_path);
    std::optional<Score> get_score(std::string& hash, int difficulty, int player_id);
    Score save_score(std::string& hash, int difficulty, int player_id, Score score);
    void add_path_binding(const fs::path& path, const std::array<std::string, 5>& hashes);
    std::array<std::string, 5>& get_hashes(const fs::path& path);
    std::string get_single_hash(const fs::path& path);
    std::optional<fs::path> get_path_by_hash(const std::string& single_hash);
    void add_song(const std::array<std::string, 5>& hash, const std::string& title, const std::string& subtitle);
    void remap_hashes(const std::unordered_map<std::string, std::string>& old_to_new);
    std::optional<PlayerData> get_player_data(int player_id);
    void save_player_data(const PlayerData& player);
    int add_player(const std::string& name);
    void begin_transaction();
    void commit();
};

extern ScoresManager scores_manager;

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
    return m;
}
