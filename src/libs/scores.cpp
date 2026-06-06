#include "scores.h"
#include "color_utils.h"
#include "song_parser.h"
#include <numeric>

ScoresManager::ScoresManager(const fs::path& db_path) {
    if (sqlite3_open(db_path.string().c_str(), &db_fsd) != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db_fsd)));
    }

    int version = 0;
    auto callback = [](void* data, int, char** argv, char**) -> int {
        *static_cast<int*>(data) = std::atoi(argv[0]);
        return 0;
    };
    sqlite3_exec(db_fsd, "PRAGMA user_version;", callback, &version, nullptr);

    if (version < 2) {
        const char* migrations[] = {
            "ALTER TABLE players ADD COLUMN modifier_auto BOOL NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN title TEXT NOT NULL DEFAULT '';",
            "ALTER TABLE players ADD COLUMN title_bg INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN dan INTEGER NOT NULL DEFAULT -1;",
            "ALTER TABLE players ADD COLUMN gold BOOL NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN rainbow BOOL NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN modifier_speed INTEGER NOT NULL DEFAULT 10;",
            "ALTER TABLE players ADD COLUMN modifier_display BOOL NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN modifier_inverse BOOL NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN modifier_random INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN neiro_index INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN chara_color_1 TEXT NOT NULL DEFAULT '#68BFC0';",
            "ALTER TABLE players ADD COLUMN chara_color_2 TEXT NOT NULL DEFAULT '#F94728';",
            "ALTER TABLE players ADD COLUMN chara_color_3 TEXT NOT NULL DEFAULT '#F9F0E1';",
            "ALTER TABLE players ADD COLUMN chara_head_index INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN chara_body_index INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN chara_cos_index INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN chara_is_costume BOOL NOT NULL DEFAULT 1;",
            "ALTER TABLE players ADD COLUMN chara_paint_index INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN chara_face_index INTEGER NOT NULL DEFAULT 0;",
            "ALTER TABLE players ADD COLUMN chara_acce_index INTEGER NOT NULL DEFAULT 0;",
        };
        for (const char* sql : migrations) {
            sqlite3_exec(db_fsd, sql, nullptr, nullptr, nullptr);
        }
    }

    sqlite3_exec(db_fsd, "PRAGMA user_version = 2;", nullptr, nullptr, nullptr);

    std::string create_players =
        "CREATE TABLE IF NOT EXISTS players"
        "(player_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "username TEXT NOT NULL UNIQUE,"
        "title TEXT NOT NULL,"
        "title_bg INTEGER NOT NULL DEFAULT 0,"
        "dan INTEGER NOT NULL DEFAULT -1,"
        "gold BOOL NOT NULL DEFAULT 0,"
        "rainbow BOOL NOT NULL DEFAULT 0,"
        "modifier_auto BOOL NOT NULL DEFAULT 0,"
        "modifier_speed INTEGER NOT NULL DEFAULT 10,"
        "modifier_display BOOL NOT NULL DEFAULT 0,"
        "modifier_inverse BOOL NOT NULL DEFAULT 0,"
        "modifier_random INTEGER NOT NULL DEFAULT 0,"
        "neiro_index INTEGER NOT NULL DEFAULT 0,"
        "chara_color_1 TEXT NOT NULL DEFAULT '#68BFC0',"
        "chara_color_2 TEXT NOT NULL DEFAULT '#F94728',"
        "chara_color_3 TEXT NOT NULL DEFAULT '#F9F0E1',"
        "chara_head_index INTEGER NOT NULL DEFAULT 0,"
        "chara_body_index INTEGER NOT NULL DEFAULT 0,"
        "chara_cos_index INTEGER NOT NULL DEFAULT 0,"
        "chara_is_costume BOOL NOT NULL DEFAULT 1,"
        "chara_paint_index INTEGER NOT NULL DEFAULT 0,"
        "chara_face_index INTEGER NOT NULL DEFAULT 0,"
        "chara_acce_index INTEGER NOT NULL DEFAULT 0);";

    std::string create_songs =
        "CREATE TABLE IF NOT EXISTS songs"
        "(song_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "hash_0 TEXT,"
        "hash_1 TEXT,"
        "hash_2 TEXT,"
        "hash_3 TEXT,"
        "hash_4 TEXT,"
        "title TEXT NOT NULL,"
        "subtitle TEXT,"
        "UNIQUE(hash_0, hash_1, hash_2, hash_3, hash_4));";

    std::string create_scores =
        "CREATE TABLE IF NOT EXISTS scores"
        "(score_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "player_id INTEGER NOT NULL REFERENCES players(player_id),"
        "hash TEXT,"
        "difficulty INTEGER NOT NULL,"
        "crown INTEGER NOT NULL,"
        "rank INTEGER NOT NULL,"
        "score INTEGER NOT NULL,"
        "good INTEGER NOT NULL,"
        "ok INTEGER NOT NULL,"
        "bad INTEGER NOT NULL,"
        "drumroll INTEGER NOT NULL,"
        "max_combo INTEGER NOT NULL);";

    char* errmsg = nullptr;
    if (sqlite3_exec(db_fsd, create_players.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        spdlog::error("Failed to create players table: {}", errmsg);
        sqlite3_free(errmsg);
    }
    if (sqlite3_exec(db_fsd, create_songs.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        spdlog::error("Failed to create songs table: {}", errmsg);
        sqlite3_free(errmsg);
    }
    if (sqlite3_exec(db_fsd, create_scores.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        spdlog::error("Failed to create scores table: {}", errmsg);
        sqlite3_free(errmsg);
    }

    sqlite3_exec(db_fsd,
        "INSERT OR IGNORE INTO players (player_id, username, title) VALUES (1, 'Don-chan', 'Donder Debut!');",
        nullptr, nullptr, nullptr);

    load_score_cache();
}

void ScoresManager::load_score_cache() {
    score_cache.clear();

    sqlite3_stmt* stmt;
    const char* query =
        "SELECT player_id, hash, difficulty, score, good, ok, bad, drumroll, max_combo, crown, rank "
        "FROM scores "
        "ORDER BY crown DESC, score DESC;";
    if (sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("load_score_cache: failed to prepare statement: {}", sqlite3_errmsg(db_fsd));
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int player_id = sqlite3_column_int(stmt, 0);
        const unsigned char* hash_text = sqlite3_column_text(stmt, 1);
        std::string hash = hash_text ? reinterpret_cast<const char*>(hash_text) : "";
        int difficulty = sqlite3_column_int(stmt, 2);

        // Rows are ordered best-first, so the first row seen for a key is the best one.
        auto key = std::make_tuple(hash, difficulty, player_id);
        if (score_cache.count(key)) continue;

        Score s;
        s.score     = sqlite3_column_int(stmt, 3);
        s.good      = sqlite3_column_int(stmt, 4);
        s.ok        = sqlite3_column_int(stmt, 5);
        s.bad       = sqlite3_column_int(stmt, 6);
        s.drumroll  = sqlite3_column_int(stmt, 7);
        s.max_combo = sqlite3_column_int(stmt, 8);
        s.crown     = static_cast<Crown>(sqlite3_column_int(stmt, 9));
        s.rank      = static_cast<Rank>(sqlite3_column_int(stmt, 10));
        score_cache.emplace(std::move(key), s);
    }
    sqlite3_finalize(stmt);
}


void ScoresManager::py_taiko_import(const fs::path& old_db_path) {
    // Build reverse lookup: (en_name, jp_name) -> new hashes
    using NameKey = std::pair<std::string, std::string>;
    struct PairHash {
        size_t operator()(const NameKey& k) const {
            return std::hash<std::string>{}(k.first) ^ (std::hash<std::string>{}(k.second) << 1);
        }
    };
    std::unordered_map<NameKey, std::array<std::string, 5>, PairHash> name_to_hashes;

    for (const auto& [path, hashes] : path_to_hashes) {
        try {
            SongParser parser(path);
            std::string en = parser.metadata.title.count("en") ? parser.metadata.title.at("en") : "";
            std::string ja = parser.metadata.title.count("ja") ? parser.metadata.title.at("ja") : "";
            name_to_hashes[{en, ja}] = hashes;
        } catch (const std::exception& e) {
            spdlog::warn("py_taiko_import: failed to parse metadata for {}: {}", path.string(), e.what());
        }
    }

    spdlog::info("py_taiko_import: built name lookup with {} entries", name_to_hashes.size());

    // Open old DB
    sqlite3* old_db;
    if (sqlite3_open(old_db_path.string().c_str(), &old_db) != SQLITE_OK) {
        spdlog::error("py_taiko_import: failed to open old DB at {}", old_db_path.string());
        return;
    }

    // Ensure default player exists
    sqlite3_exec(db_fsd,
        "INSERT OR IGNORE INTO players (player_id, username, title) VALUES (1, 'Don-chan', 'Donder Debut!');",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* sel;
    const char* select_query =
        "SELECT en_name, jp_name, diff, score, good, ok, bad, drumroll, combo, clear "
        "FROM Scores;";

    if (sqlite3_prepare_v2(old_db, select_query, -1, &sel, nullptr) != SQLITE_OK) {
        spdlog::error("py_taiko_import: failed to prepare SELECT: {}", sqlite3_errmsg(old_db));
        sqlite3_close(old_db);
        return;
    }

    int imported = 0, skipped = 0;

    while (sqlite3_step(sel) == SQLITE_ROW) {
        const char* en_raw = reinterpret_cast<const char*>(sqlite3_column_text(sel, 0));
        const char* ja_raw = reinterpret_cast<const char*>(sqlite3_column_text(sel, 1));
        int diff           = sqlite3_column_int(sel, 2);
        int score_val      = sqlite3_column_int(sel, 3);
        int good           = sqlite3_column_int(sel, 4);
        int ok             = sqlite3_column_int(sel, 5);
        int bad            = sqlite3_column_int(sel, 6);
        int drumroll       = sqlite3_column_int(sel, 7);
        int combo          = sqlite3_column_int(sel, 8);
        int crown_val      = sqlite3_column_int(sel, 9);

        if (!en_raw || diff < 0 || diff > 4) {
            spdlog::warn("py_taiko_import: skipping row with null en_name or invalid diff {}", diff);
            skipped++;
            continue;
        }

        std::string en = en_raw;
        std::string ja = ja_raw ? ja_raw : "";

        auto it = name_to_hashes.find({en, ja});
        if (it == name_to_hashes.end()) {
            spdlog::warn("py_taiko_import: no hash match found for '{}' / '{}', skipping", en, ja);
            skipped++;
            continue;
        }

        const std::string& new_hash = it->second[diff];
        if (new_hash.empty()) {
            spdlog::warn("py_taiko_import: new hash empty for '{}' diff {}, skipping", en, diff);
            skipped++;
            continue;
        }

        // Duplicate check
        {
            sqlite3_stmt* check_stmt;
            char check_query[256];
            snprintf(check_query, sizeof(check_query),
                "SELECT 1 FROM scores WHERE player_id = 1 AND hash = ? AND difficulty = ? LIMIT 1;");
            if (sqlite3_prepare_v2(db_fsd, check_query, -1, &check_stmt, nullptr) != SQLITE_OK) {
                spdlog::warn("py_taiko_import: failed to prepare check statement, skipping");
                skipped++;
                continue;
            }
            sqlite3_bind_text(check_stmt, 1, new_hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(check_stmt,  2, diff);
            bool exists = (sqlite3_step(check_stmt) == SQLITE_ROW);
            sqlite3_finalize(check_stmt);

            if (exists) {
                spdlog::debug("py_taiko_import: score already exists for '{}' diff {}, skipping", en, diff);
                skipped++;
                continue;
            }
        }

        // Insert score
        {
            sqlite3_stmt* ins_stmt;
            char ins_query[512];
            snprintf(ins_query, sizeof(ins_query),
                "INSERT INTO scores "
                "(player_id, hash, difficulty, score, good, ok, bad, drumroll, max_combo, crown, rank) "
                "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0);");
            if (sqlite3_prepare_v2(db_fsd, ins_query, -1, &ins_stmt, nullptr) != SQLITE_OK) {
                spdlog::warn("py_taiko_import: failed to prepare insert statement, skipping");
                skipped++;
                continue;
            }
            sqlite3_bind_text(ins_stmt, 1, new_hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(ins_stmt,  2, diff);
            sqlite3_bind_int(ins_stmt,  3, score_val);
            sqlite3_bind_int(ins_stmt,  4, good);
            sqlite3_bind_int(ins_stmt,  5, ok);
            sqlite3_bind_int(ins_stmt,  6, bad);
            sqlite3_bind_int(ins_stmt,  7, drumroll);
            sqlite3_bind_int(ins_stmt,  8, combo);
            sqlite3_bind_int(ins_stmt,  9, crown_val);
            sqlite3_step(ins_stmt);
            sqlite3_finalize(ins_stmt);
            imported++;
        }
    }

    sqlite3_finalize(sel);
    sqlite3_close(old_db);
    spdlog::info("py_taiko_import: done — {} imported, {} skipped", imported, skipped);

    if (imported > 0) load_score_cache();
}

std::optional<Score> ScoresManager::get_score(std::string& hash, int difficulty, int player_id) {
    auto it = score_cache.find(std::make_tuple(hash, difficulty, player_id));
    if (it != score_cache.end()) return it->second;
    return std::nullopt;
}

Score ScoresManager::save_score(std::string& hash, int difficulty, int player_id, Score score) {
    sqlite3_stmt* stmt;

    char query[512];
    snprintf(query, sizeof(query),
        "INSERT INTO scores (player_id, hash, difficulty, score, good, ok, bad, drumroll, max_combo, crown, rank) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");

    if (sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("save_score: failed to prepare statement: {}", sqlite3_errmsg(db_fsd));
        return score;
    }

    sqlite3_bind_int(stmt, 1, player_id);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, nullptr);
    sqlite3_bind_int(stmt, 3, difficulty);
    sqlite3_bind_int(stmt, 4, score.score);
    sqlite3_bind_int(stmt, 5, score.good);
    sqlite3_bind_int(stmt, 6, score.ok);
    sqlite3_bind_int(stmt, 7, score.bad);
    sqlite3_bind_int(stmt, 8, score.drumroll);
    sqlite3_bind_int(stmt, 9, score.max_combo);
    sqlite3_bind_int(stmt, 10, static_cast<int>(score.crown));
    sqlite3_bind_int(stmt, 11, static_cast<int>(score.rank));

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    spdlog::info("Saved score for hash: {} score: {} crown: {}", hash, score.score, (int)score.crown);

    auto key = std::make_tuple(hash, difficulty, player_id);
    auto it = score_cache.find(key);
    bool is_better = it == score_cache.end() ||
        score.crown > it->second.crown ||
        (score.crown == it->second.crown && score.score > it->second.score);
    if (is_better) score_cache[key] = score;

    return score;
}

void ScoresManager::add_path_binding(const fs::path& path, const std::array<std::string, 5>& hashes) {
    path_to_hashes[path] = hashes;
    std::string single = std::accumulate(hashes.begin(), hashes.end(), std::string{});
    single_hash_to_path[single] = path;
}

std::optional<fs::path> ScoresManager::get_path_by_hash(const std::string& single_hash) {
    auto it = single_hash_to_path.find(single_hash);
    if (it != single_hash_to_path.end()) return it->second;
    return std::nullopt;
}

std::array<std::string, 5>& ScoresManager::get_hashes(const fs::path& path) {
    return path_to_hashes[path];
}

std::string ScoresManager::get_single_hash(const fs::path& path) {
    return std::accumulate(path_to_hashes[path].begin(), path_to_hashes[path].end(), std::string{});
}

void ScoresManager::add_song(const std::array<std::string, 5>& hashes, const std::string& title, const std::string& subtitle) {
    sqlite3_stmt* stmt;
    const char* query =
        "INSERT OR IGNORE INTO songs (title, subtitle, hash_0, hash_1, hash_2, hash_3, hash_4) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("add_song: failed to prepare statement: {}", sqlite3_errmsg(db_fsd));
        return;
    }
    sqlite3_bind_text(stmt, 1, title.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, subtitle.c_str(), -1, SQLITE_STATIC);
    for (int i = 0; i < 5; i++) {
        sqlite3_bind_text(stmt, 3 + i, hashes[i].c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ScoresManager::remap_hashes(const std::unordered_map<std::string, std::string>& old_to_new) {
    if (old_to_new.empty()) return;

    sqlite3_exec(db_fsd, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    sqlite3_stmt* scores_stmt;
    if (sqlite3_prepare_v2(db_fsd,
            "UPDATE scores SET hash = ? WHERE hash = ?;",
            -1, &scores_stmt, nullptr) != SQLITE_OK) {
        spdlog::error("remap_hashes: failed to prepare scores statement: {}", sqlite3_errmsg(db_fsd));
        sqlite3_exec(db_fsd, "ROLLBACK;", nullptr, nullptr, nullptr);
        return;
    }

    std::array<sqlite3_stmt*, 5> song_stmts;
    for (int i = 0; i < 5; i++) {
        std::string q = "UPDATE songs SET hash_" + std::to_string(i)
                      + " = ? WHERE hash_" + std::to_string(i) + " = ?;";
        if (sqlite3_prepare_v2(db_fsd, q.c_str(), -1, &song_stmts[i], nullptr) != SQLITE_OK) {
            spdlog::error("remap_hashes: failed to prepare song statement {}: {}", i, sqlite3_errmsg(db_fsd));
            sqlite3_finalize(scores_stmt);
            for (int j = 0; j < i; j++) sqlite3_finalize(song_stmts[j]);
            sqlite3_exec(db_fsd, "ROLLBACK;", nullptr, nullptr, nullptr);
            return;
        }
    }

    for (const auto& [old_h, new_h] : old_to_new) {
        sqlite3_bind_text(scores_stmt, 1, new_h.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(scores_stmt, 2, old_h.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(scores_stmt);
        sqlite3_reset(scores_stmt);

        for (int i = 0; i < 5; i++) {
            sqlite3_bind_text(song_stmts[i], 1, new_h.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(song_stmts[i], 2, old_h.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(song_stmts[i]);
            sqlite3_reset(song_stmts[i]);
        }
    }

    sqlite3_finalize(scores_stmt);
    for (auto& s : song_stmts) sqlite3_finalize(s);

    sqlite3_exec(db_fsd, "COMMIT;", nullptr, nullptr, nullptr);
    spdlog::info("remap_hashes: remapped {} hash pairs", old_to_new.size());

    load_score_cache();
}

int ScoresManager::add_player(const std::string& name) {
    sqlite3_stmt* stmt;

    const char* insert_query =
        "INSERT OR IGNORE INTO players (username) "
        "VALUES (?);";
    if (sqlite3_prepare_v2(db_fsd, insert_query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("add_player: failed to prepare insert: {}", sqlite3_errmsg(db_fsd));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* select_query =
        "SELECT player_id FROM players WHERE username = ?;";
    if (sqlite3_prepare_v2(db_fsd, select_query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("add_player: failed to prepare select: {}", sqlite3_errmsg(db_fsd));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return id;
}

std::optional<PlayerData> ScoresManager::get_player_data(int player_id) {
    sqlite3_stmt* stmt;
    const char* query =
        "SELECT player_id, username, title, title_bg, dan, gold, rainbow,"
        " modifier_auto, modifier_speed, modifier_display, modifier_inverse, modifier_random,"
        " neiro_index, chara_color_1, chara_color_2, chara_color_3,"
        " chara_head_index, chara_body_index, chara_cos_index, chara_is_costume,"
        " chara_paint_index, chara_face_index, chara_acce_index"
        " FROM players WHERE player_id = ?;";

    if (sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("get_player: failed to prepare: {}", sqlite3_errmsg(db_fsd));
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, player_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);

        // Player not found — insert a default row with the requested player_id
        sqlite3_stmt* ins;
        const char* ins_query =
            "INSERT OR IGNORE INTO players (player_id, username, title) VALUES (?, ?, 'Donder Debut!');";
        if (sqlite3_prepare_v2(db_fsd, ins_query, -1, &ins, nullptr) == SQLITE_OK) {
            std::string default_name = "Player " + std::to_string(player_id);
            sqlite3_bind_int (ins, 1, player_id);
            sqlite3_bind_text(ins, 2, default_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(ins);
            sqlite3_finalize(ins);
        } else {
            spdlog::error("get_player: failed to insert default player {}: {}", player_id, sqlite3_errmsg(db_fsd));
            return std::nullopt;
        }

        // Re-query the newly inserted row
        if (sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr) != SQLITE_OK) {
            spdlog::error("get_player: failed to re-prepare after insert: {}", sqlite3_errmsg(db_fsd));
            return std::nullopt;
        }
        sqlite3_bind_int(stmt, 1, player_id);
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }
    }

    auto col_str = [&](int i) -> std::string {
        auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
        return p ? p : "";
    };

    PlayerData p;
    p.player_id       = sqlite3_column_int(stmt, 0);
    p.username        = col_str(1);
    p.title           = col_str(2);
    p.title_bg        = sqlite3_column_int(stmt, 3);
    p.dan             = sqlite3_column_int(stmt, 4);
    p.gold            = sqlite3_column_int(stmt, 5);
    p.rainbow         = sqlite3_column_int(stmt, 6);
    p.modifier_auto   = sqlite3_column_int(stmt, 7);
    p.modifier_speed  = sqlite3_column_int(stmt, 8);
    p.modifier_display  = sqlite3_column_int(stmt, 9);
    p.modifier_inverse  = sqlite3_column_int(stmt, 10);
    p.modifier_random   = sqlite3_column_int(stmt, 11);
    p.neiro_index       = sqlite3_column_int(stmt, 12);
    p.chara_color_1     = parse_hex_color(col_str(13));
    p.chara_color_2     = parse_hex_color(col_str(14));
    p.chara_color_3     = parse_hex_color(col_str(15));
    p.chara_head_index  = sqlite3_column_int(stmt, 16);
    p.chara_body_index  = sqlite3_column_int(stmt, 17);
    p.chara_cos_index   = sqlite3_column_int(stmt, 18);
    p.chara_is_costume  = sqlite3_column_int(stmt, 19);
    p.chara_paint_index = sqlite3_column_int(stmt, 20);
    p.chara_face_index  = sqlite3_column_int(stmt, 21);
    p.chara_acce_index  = sqlite3_column_int(stmt, 22);

    sqlite3_finalize(stmt);
    return p;
}

void ScoresManager::save_player_data(const PlayerData& player) {
    sqlite3_stmt* stmt;
    const char* query =
        "UPDATE players SET"
        " username = ?, title = ?, title_bg = ?, dan = ?, gold = ?, rainbow = ?,"
        " modifier_auto = ?, modifier_speed = ?, modifier_display = ?, modifier_inverse = ?, modifier_random = ?,"
        " neiro_index = ?, chara_color_1 = ?, chara_color_2 = ?, chara_color_3 = ?,"
        " chara_head_index = ?, chara_body_index = ?, chara_cos_index = ?, chara_is_costume = ?,"
        " chara_paint_index = ?, chara_face_index = ?, chara_acce_index = ?"
        " WHERE player_id = ?;";

    if (sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("save_player: failed to prepare: {}", sqlite3_errmsg(db_fsd));
        return;
    }

    std::string c1 = color_to_hex(player.chara_color_1);
    std::string c2 = color_to_hex(player.chara_color_2);
    std::string c3 = color_to_hex(player.chara_color_3);

    sqlite3_bind_text(stmt,  1, player.username.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  2, player.title.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt,  3, player.title_bg);
    sqlite3_bind_int (stmt,  4, player.dan);
    sqlite3_bind_int (stmt,  5, player.gold);
    sqlite3_bind_int (stmt,  6, player.rainbow);
    sqlite3_bind_int (stmt,  7, player.modifier_auto);
    sqlite3_bind_int (stmt,  8, player.modifier_speed);
    sqlite3_bind_int (stmt,  9, player.modifier_display);
    sqlite3_bind_int (stmt, 10, player.modifier_inverse);
    sqlite3_bind_int (stmt, 11, player.modifier_random);
    sqlite3_bind_int (stmt, 12, player.neiro_index);
    sqlite3_bind_text(stmt, 13, c1.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, c2.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 15, c3.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 16, player.chara_head_index);
    sqlite3_bind_int (stmt, 17, player.chara_body_index);
    sqlite3_bind_int (stmt, 18, player.chara_cos_index);
    sqlite3_bind_int (stmt, 19, player.chara_is_costume);
    sqlite3_bind_int (stmt, 20, player.chara_paint_index);
    sqlite3_bind_int (stmt, 21, player.chara_face_index);
    sqlite3_bind_int (stmt, 22, player.chara_acce_index);
    sqlite3_bind_int (stmt, 23, player.player_id);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        spdlog::error("save_player: failed to update player {}: {}", player.player_id, sqlite3_errmsg(db_fsd));

    sqlite3_finalize(stmt);
}

void ScoresManager::begin_transaction() {
    sqlite3_exec(db_fsd, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
}

void ScoresManager::commit() {
    sqlite3_exec(db_fsd, "COMMIT;", nullptr, nullptr, nullptr);
}

//std::string score_path = (global_data.config->general.score_method == ScoreMethod::GEN3) ? "scores_gen3.db" : "scores.db";

ScoresManager scores_manager("scores.db");
