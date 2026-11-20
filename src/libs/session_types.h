#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

struct Exam {
    std::string type; // "gauge","combo","judgebad","judgegood","judgeperfect","hit","score"
    int red = 0;
    int gold = 0;
    std::string range; // "less" or "more"
    bool gothrough = true;
    std::vector<int> song_red;
    std::vector<int> song_gold;
    static constexpr int GOLD_FULL = -1;
    bool per_song() const { return !song_red.empty(); }
    // The exam as it applies to song i: red/gold swapped for that song's pair.
    Exam for_song(int i) const {
        assert(!per_song() || (i >= 0 && i < std::ssize(song_red)));
        Exam ex = *this;
        if (per_song() && i >= 0 && i < std::ssize(song_red)) {
            ex.red = song_red[i];
            ex.gold = i < std::ssize(song_gold) ? song_gold[i] : song_red[i];
        }
        // the view of one song is a plain pair again (captions print a single number)
        ex.song_red.clear();
        ex.song_gold.clear();
        return ex;
    }
};

inline std::string_view dan_bar_state(const Exam& exam, int value, bool live = false, bool near_end = false,
                                      bool just_before_end = false) {
    const bool down = (exam.range == "less");
    // CheckMax
    if (exam.gold > 0 && ((down && value < exam.gold) || (!down && value >= exam.gold))) {
        if (!live || !down) {
            return "max";
        }
        if (just_before_end) {
            return "max_soon2";
        }
        if (near_end) {
            return "max_soon";
        }
        // fall through to the flat palette -- no `max` during play
    }
    int gauge = (exam.red > 0) ? static_cast<int>(std::floor(100.0 * value / exam.red)) : 100;
    gauge = std::clamp(gauge, 0, 100);
    if (down) {
        gauge = 100 - gauge;
    }
    if (gauge <= 0) {
        return "empty";
    }
    if (!down) {
        if (gauge <= 49) {
            return "up_50";
        }
        if (gauge <= 99) {
            return "up_80";
        }
        return "up_100";
    }
    const int good =
        (exam.red > 0 && exam.gold > 0) ? 100 - static_cast<int>(std::floor(100.0 * exam.gold / exam.red)) : 100;
    if (gauge < 30) {
        return "down_80";
    }
    if (gauge <= good) {
        return "up_80";
    }
    return "down_100";
}

struct DanSongEntry {
    std::filesystem::path song_path;
    int genre_index = 0;
    int difficulty = 0;
    int level = 0;
    bool hidden = false;
};

// the course picked on dan select
struct DanCourse {
    std::vector<DanSongEntry> songs;
    std::vector<Exam> exams;
    int color = 0;
    int rank = -1;
    int index = -1;
    int index_max = -1;
    bool gaiden = false;
    bool modern = true;
    bool gen3 = false;
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
    int64_t score_id = -1;
};

struct DanResultExam {
    float progress = 0.0f;
    int counter_value = 0;
    std::string bar_texture = "exam_red";
    bool failed = false;
    int song_value[3] = {0, 0, 0};
    float song_progress[3] = {0.0f, 0.0f, 0.0f};
    int song_count = 0;
    int tier = 0;
    std::string bar_state = "empty";
    std::string song_state[3] = {"empty", "empty", "empty"};
};

struct DanResultData {
    DanCourse course;
    std::string dan_title = "default_title";
    int score = 0;
    float gauge_length = 0.0f;
    int max_combo = 0;
    std::vector<DanResultSong> songs;
    std::vector<DanResultExam> exam_data;
    int odai_result = -1;
    bool skipped = false; // the player skipped out: nothing is recorded
};

struct ResultData {
    int score = 0;
    int good = 0;
    int ok = 0;
    int bad = 0;
    int max_combo = 0;
    int total_drumroll = 0;
    float gauge_length = 0.0f;
    bool cleared = true;
    int prev_score = 0;
    bool submitted = false;
    double hit_offset_sum_ms = 0.0;
    int hit_offset_count = 0;
};

struct AISectionTally {
    int good = 0;
    int ok = 0;
    int bad = 0;
    int drumroll = 0;
    bool won = false;
};

struct AIMatchResult {
    std::vector<AISectionTally> sections;
    bool won = false;
    bool saves = false;
    int stars = 1;
};

// one battle mode song, for its result screen
struct BattleTally {
    bool played = false;
    int stage = 1;
    int difficulty = 0;
    bool boss = false;
    int boss_hp = 0; // before the song
    int boss_hp_max = 0;
    int kills = 0;
    int damage = 0;
    int power = 0;
    int chests = 0;
    int orb_chests = 0;
};
