#include "scores.h"

ScoresManager::ScoresManager(const fs::path& db_path) {
    sqlite3_open(db_path.string().c_str(), &db_fsd);

    int version = 0;
    auto callback = [](void* data, int, char** argv, char**) -> int {
        *static_cast<int*>(data) = std::atoi(argv[0]);
        return 0;
    };
    sqlite3_exec(db_fsd, "PRAGMA user_version;", callback, &version, nullptr);

    sqlite3_exec(db_fsd, "PRAGMA user_version = 1;", nullptr, nullptr, nullptr);

    std::string create_players =
        "CREATE TABLE IF NOT EXISTS players"
        "(player_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL UNIQUE,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";

    std::string create_songs =
        "CREATE TABLE IF NOT EXISTS songs"
        "(song_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "hash_0 TEXT,"
        "hash_1 TEXT,"
        "hash_2 TEXT,"
        "hash_3 TEXT,"
        "hash_4 TEXT,"
        "TITLE TEXT NOT NULL,"
        "SUBTITLE TEXT);";

    std::string create_scores =
        "CREATE TABLE IF NOT EXISTS scores"
        "(score_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "player_id INTEGER NOT NULL REFERENCES players(player_id),"
        "hash_0 TEXT REFERENCES songs(hash_0),"
        "hash_1 TEXT REFERENCES songs(hash_1),"
        "hash_2 TEXT REFERENCES songs(hash_2),"
        "hash_3 TEXT REFERENCES songs(hash_3),"
        "hash_4 TEXT REFERENCES songs(hash_4),"
        "difficulty INTEGER NOT NULL,"
        "crown INTEGER NOT NULL,"
        "rank INTEGER NOT NULL,"
        "score INTEGER NOT NULL,"
        "good INTEGER NOT NULL,"
        "ok INTEGER NOT NULL,"
        "bad INTEGER NOT NULL,"
        "drumroll INTEGER NOT NULL,"
        "max_combo INTEGER NOT NULL);";

    sqlite3_exec(db_fsd, create_players.c_str(), nullptr, nullptr, nullptr);
    sqlite3_exec(db_fsd, create_songs.c_str(), nullptr, nullptr, nullptr);
    sqlite3_exec(db_fsd, create_scores.c_str(), nullptr, nullptr, nullptr);

}

static std::string hash_col(int difficulty) {
    return "hash_" + std::to_string(difficulty);
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
            TJAParser parser(path);
            parser.get_metadata();
            std::string en = parser.metadata.title["en"];
            std::string ja = parser.metadata.title["ja"];
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
        "INSERT OR IGNORE INTO players (player_id, username) VALUES (1, 'Player 1');",
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
                "SELECT 1 FROM scores WHERE player_id = 1 AND %s = ? AND difficulty = ? LIMIT 1;",
                hash_col(diff).c_str());
            sqlite3_prepare_v2(db_fsd, check_query, -1, &check_stmt, nullptr);
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
                "(player_id, %s, difficulty, score, good, ok, bad, drumroll, max_combo, crown, rank) "
                "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0);",
                hash_col(diff).c_str());
            sqlite3_prepare_v2(db_fsd, ins_query, -1, &ins_stmt, nullptr);
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
}

std::optional<Score> ScoresManager::get_score(std::string& hash, int difficulty, int player_id) {
    sqlite3_stmt* stmt;
    char query[256];
    snprintf(query, sizeof(query),
        "SELECT score, good, ok, bad, drumroll, max_combo, crown, rank "
        "FROM scores "
        "WHERE player_id = ? AND %s = ? "
        "LIMIT 1;",
        hash_col(difficulty).c_str());
    sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, player_id);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Score result;
        result.score     = sqlite3_column_int(stmt, 0);
        result.good      = sqlite3_column_int(stmt, 1);
        result.ok        = sqlite3_column_int(stmt, 2);
        result.bad       = sqlite3_column_int(stmt, 3);
        result.drumroll  = sqlite3_column_int(stmt, 4);
        result.max_combo = sqlite3_column_int(stmt, 5);
        result.crown     = static_cast<Crown>(sqlite3_column_int(stmt, 6));
        result.rank      = static_cast<Rank>(sqlite3_column_int(stmt, 7));
        sqlite3_finalize(stmt);
        return result;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

Score ScoresManager::save_score(std::string& hash, int difficulty, int player_id, Score score) {
    sqlite3_stmt* stmt;

    char query[512];
    snprintf(query, sizeof(query),
        "INSERT INTO scores (player_id, %s, difficulty, score, good, ok, bad, drumroll, max_combo, crown, rank) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
        hash_col(difficulty).c_str());

    sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr);

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
    return score;
}

void ScoresManager::add_path_binding(const fs::path& path, const std::array<std::string, 5>& hashes) {
    path_to_hashes[path] = hashes;
}

std::array<std::string, 5>& ScoresManager::get_hashes(const fs::path& path) {
    return path_to_hashes[path];
}

void ScoresManager::add_song(const std::array<std::string, 5>& hashes, const std::string& title, const std::string& subtitle) {
    sqlite3_stmt* stmt;
    const char* query =
        "INSERT OR IGNORE INTO songs (title, subtitle, hash_0, hash_1, hash_2, hash_3, hash_4) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db_fsd, query, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, title.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, subtitle.c_str(), -1, SQLITE_STATIC);
    for (int i = 0; i < 5; i++) {
        sqlite3_bind_text(stmt, 3 + i, hashes[i].c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int ScoresManager::add_player(const std::string& name) {
    sqlite3_stmt* stmt;

    const char* insert_query =
        "INSERT OR IGNORE INTO players (username) "
        "VALUES (?);";
    sqlite3_prepare_v2(db_fsd, insert_query, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* select_query =
        "SELECT id FROM players WHERE username = ?;";
    sqlite3_prepare_v2(db_fsd, select_query, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return id;
}

ScoresManager scores_manager("scores.db");
