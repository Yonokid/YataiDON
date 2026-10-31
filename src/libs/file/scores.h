#pragma once

#include <cassert>
#include <chrono>
#include <mutex>
#include <sqlite3.h>

#include "../gfx/color_utils.h"
#include "../global_data.h"
#include "../parsers/tja.h"

inline int64_t unix_now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

struct PlayerData {
    std::string player_id;
    std::string username;
    std::string title;
    int title_bg = 0;
    int dan = -1; // -1 = no 段位, matches the column default
    bool gold = false;
    bool rainbow = false;
    bool modifier_auto = false;
    int modifier_speed = 10; // 1.0x, in tenths
    bool modifier_display = false;
    bool modifier_inverse = false;
    int modifier_random = 0;
    bool modifier_skip = false;
    int neiro_index = 0;
    Color chara_color_1 = Color{0x68, 0xBF, 0xC0, 0xFF};
    Color chara_color_2 = Color{0xF9, 0x47, 0x28, 0xFF};
    Color chara_color_3 = Color{0xF9, 0xF0, 0xE1, 0xFF};
    int chara_head_index = 0;
    int chara_body_index = 0;
    int chara_cos_index = 0;
    bool chara_is_costume = true;
    int chara_paint_index = 0;
    int chara_face_index = 0;
    int chara_acce_index = 0;
};

struct DanRecord {
    int dan_index = -1;
    int rank = 0;
    int score = 0;
    int arrival = 0;
};

inline Rank rank_for_score(int score) {
    if (score >= 1000000) {
        return Rank::_RAINBOW;
    }
    if (score >= 950000) {
        return Rank::_PURPLE;
    }
    if (score >= 900000) {
        return Rank::_PINK;
    }
    if (score >= 800000) {
        return Rank::_GOLD;
    }
    if (score >= 700000) {
        return Rank::_SILVER;
    }
    if (score >= 600000) {
        return Rank::_BRONZE;
    }
    return Rank::_WHITE;
}

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

// What the loading screen needs from a chart file, cached in song_cache so unchanged files
// are not parsed again at the next start (see LoadingScreen::load_song_hashes).
struct SongCacheEntry {
    int64_t mtime = 0;
    int64_t size = 0;
    std::array<std::string, 5> hashes;
    std::string title;
    std::string subtitle;
    std::vector<std::pair<std::string, std::string>> titles;
    std::vector<std::pair<std::string, std::string>> subtitles;
    std::array<int, 5> levels = {-1, -1, -1, -1, -1};
};

// Bump when get_diff_hash / the parsers produce different hashes or metadata for the same file,
// so every chart is parsed again.
constexpr int SONG_CACHE_VERSION = 3; // 3: Shift-JIS charts read as UTF-8

struct RemoteScore;

class ScoresManager {
private:
    sqlite3* db_fsd;
    sqlite3_stmt* add_song_stmt = nullptr;
    mutable std::mutex maps_mutex;
    std::unordered_map<fs::path, std::array<std::string, 5>> path_to_hashes;
    std::unordered_map<std::string, fs::path> single_hash_to_path;
    std::unordered_map<std::string, fs::path> diff_hash_to_path;
    std::map<std::tuple<std::string, int, std::string>, Score> score_cache;
    std::map<std::pair<std::string, std::string>, int> play_count_cache; // (diff hash, player_id) -> plays
    void load_score_cache();

public:
    std::string player_1;
    std::string player_2;
    PlayerData player_1_data;
    PlayerData player_2_data;
    ScoresManager(const fs::path& db_path);
    ~ScoresManager();
    ScoresManager(const ScoresManager&) = delete;
    ScoresManager& operator=(const ScoresManager&) = delete;
    void py_taiko_import(const fs::path& old_db_path);
    void export_to_hiroba(const std::string& access_code, const std::string& player_id);

    int apply_remote_scores(const std::vector<RemoteScore>& scores, const std::string& player_id);
    std::optional<Score> get_score(const std::string& hash, int difficulty, const std::string& player_id);
    // diff hash -> number of saved plays for player_id
    std::unordered_map<std::string, int> get_play_counts(const std::string& player_id);
    // out_score_id (optional) receives the new scores row id, or -1 if the insert failed.
    Score save_score(const std::string& hash, int difficulty, const std::string& player_id, Score score,
                     int64_t played_at, const std::string& modifiers_json, int64_t* out_score_id = nullptr);
    void add_path_binding(const fs::path& path, const std::array<std::string, 5>& hashes);
    std::array<std::string, 5> get_hashes(const fs::path& path);
    std::string get_single_hash(const fs::path& path);
    std::optional<fs::path> get_path_by_hash(const std::string& single_hash);
    std::optional<fs::path> get_path_by_diff_hash(const std::string& diff_hash);
    void add_song(const std::array<std::string, 5>& hash, const std::string& title, const std::string& subtitle);
    void remap_hashes(const std::unordered_map<std::string, std::string>& old_to_new);
    // song_cache: path -> entry, current SONG_CACHE_VERSION only
    std::unordered_map<std::string, SongCacheEntry> load_song_cache();
    void store_song_cache(const std::string& path, const SongCacheEntry& entry);
    void remove_song_cache(const std::string& path);
    // Best-of view over dan_scores for one course (course_id from dan_course_id).
    std::optional<DanRecord> get_dan_record(const std::string& player_id, const std::string& course_id);
    // Appends one attempt to dan_scores; each song links to its normal scores row (DanResultSong::score_id).
    void save_dan_score(const std::string& player_id, const std::string& course_id, const DanResultData& rd,
                        int arrival);
    std::optional<PlayerData> get_player_data(const std::string& player_id);
    void save_player_data(const PlayerData& player);
    void migrate_player_id(const std::string& old_id, const std::string& new_id);
    bool begin_transaction();
    bool commit();
    void rollback();
};

// Chart hash of one dan song (path binding, else parsed from the chart).
std::string dan_song_hash(const DanSongEntry& song);

// Identity of a dan course: sha256 over its songs' chart hashes + difficulties and its exams.
std::string dan_course_id(const std::vector<DanSongEntry>& songs, const std::vector<Exam>& exams);

extern ScoresManager* _scores_manager_ptr;
inline ScoresManager& get_scores_manager() {
    assert(_scores_manager_ptr && "scores_manager used before init_scores_manager()");
    return *_scores_manager_ptr;
}
#define scores_manager get_scores_manager()
void init_scores_manager(bool gen3);

inline Color chara_default_color_1(PlayerNum player_num) {
    return (player_num == PlayerNum::P1) ? Color{104, 191, 192, 255} : Color{249, 71, 40, 255};
}
inline Color chara_default_color_2(PlayerNum player_num) {
    return (player_num == PlayerNum::P1) ? Color{249, 71, 40, 255} : Color{104, 191, 192, 255};
}

inline Modifiers player_data_to_modifiers(const PlayerData& pd) {
    Modifiers m;
    m.auto_play = pd.modifier_auto;
    m.speed = pd.modifier_speed;
    m.display = pd.modifier_display;
    m.inverse = pd.modifier_inverse;
    m.random = pd.modifier_random;
    m.skip = pd.modifier_skip;
    return m;
}
