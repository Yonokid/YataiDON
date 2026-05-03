#pragma once

#include "global_data.h"
#include <sqlite3.h>

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
public:
    int player_1;
    int player_2;
    ScoresManager(const fs::path& db_path);
    void py_taiko_import(const fs::path& old_db_path);
    std::optional<Score> get_score(std::string& hash, int difficulty, int player_id);
    Score save_score(std::string& hash, int difficulty, int player_id, Score score);
    void add_path_binding(const fs::path& path, const std::array<std::string, 5>& hashes);
    std::array<std::string, 5>& get_hashes(const fs::path& path);
    std::string get_single_hash(const fs::path& path);
    std::optional<fs::path> get_path_by_hash(const std::string& single_hash);
    void add_song(const std::array<std::string, 5>& hash, const std::string& title, const std::string& subtitle);
    int add_player(const std::string& name);
};

extern ScoresManager scores_manager;
