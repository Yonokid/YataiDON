#pragma once

#include <string>
#include <set>
#include <array>

#include "../libs/global_data.h"
#include "../texture.h"

enum class Screens {
    TITLE,
    ENTRY,
    SONG_SELECT,
    GAME,
    GAME_2P,
    RESULT,
    RESULT_2P,
    SONG_SELECT_2P,
    DAN_SELECT,
    GAME_DAN,
    DAN_RESULT,
    PRACTICE_SELECT,
    GAME_PRACTICE,
    AI_SELECT,
    AI_GAME,
    SETTINGS,
    DEV_MENU,
    LOADING
};

enum class DrumType {
    DON = 1,
    KAT = 2
};

enum class Side {
    LEFT = 1,
    RIGHT = 2
};

enum class Judgments {
    GOOD = 0,
    OK = 1,
    BAD = 2
};

enum class BranchDifficulty {
    NORMAL = 0,
    EXPERT = 1,
    MASTER = 2
};

enum ResultState {
    FAIL = 0,
    CLEAR = 1,
    RAINBOW = 2,
};

enum SongSelectState {
    BROWSING = 0,
    SONG_SELECTED = 1,
    DIFF_SORTING = 2,
    SEARCHING = 3
};

inline std::string branch_diff_to_string(BranchDifficulty difficulty) {
    static const std::array<std::string, 3> names = {
        "normal",   // 0
        "expert",   // 1
        "master"    // 2
    };
    return names[static_cast<int>(difficulty)];
}

inline std::string screens_to_string(Screens screen) {
    static const std::array<std::string, 24> names = {
        "TITLE",
        "ENTRY",
        "SONG_SELECT",
        "GAME",
        "GAME_2P",
        "RESULT",
        "RESULT_2P",
        "SONG_SELECT_2P",
        "DAN_SELECT",
        "GAME_DAN",
        "DAN_RESULT",
        "PRACTICE_SELECT",
        "GAME_PRACTICE",
        "AI_SELECT",
        "AI_GAME",
        "SETTINGS",
        "DEV_MENU",
        "LOADING",
        "LOADING",
        "LOADING",
        "LOADING",
        "LOADING",
        "LOADING",
        "LOADING"
    };
    return names[static_cast<int>(screen)];
}

enum class TextureIndex : int {
    NONE       = 0,
    VOCALOID    = 1,
    DEFAULT     = 2,
    RECOMMENDED = 3,
    FAVORITE    = 4,
    RECENT      = 5
};

enum class GenreIndex : int {
    TUTORIAL    = 0,
    JPOP        = 1,
    ANIME       = 2,
    VOCALOID    = 3,
    CHILDREN    = 4,
    VARIETY     = 5,
    CLASSICAL   = 6,
    GAME        = 7,
    NAMCO       = 8,
    DEFAULT     = 9,
    RECOMMENDED = 10,
    FAVORITE    = 11,
    RECENT      = 12,
    DAN         = 13,
    DIFFICULTY  = 14
};

const std::map<std::string, TextureIndex> TEXTURE_MAP = {
    {"VOCALOID",    TextureIndex::VOCALOID},
    {"ボーカロイド", TextureIndex::VOCALOID},
    {"RECOMMENDED", TextureIndex::RECOMMENDED},
    {"FAVORITE",    TextureIndex::FAVORITE},
    {"RECENT",      TextureIndex::RECENT},
};

const std::map<GenreIndex, std::set<std::string>> GENRE_MAP = {
    { GenreIndex::TUTORIAL,   {"TUTORIAL"} },
    { GenreIndex::JPOP,       {"J-POP"} },
    { GenreIndex::ANIME,      {"ANIME", "アニメ"} },
    { GenreIndex::CHILDREN,   {"CHILDREN", "どうよう"} },
    { GenreIndex::VOCALOID,   {"VOCALOID", "ボーカロイド"} },
    { GenreIndex::VARIETY,    {"VARIETY", "バラエティー", "バラエティ"} },
    { GenreIndex::CLASSICAL,  {"CLASSICAL", "クラシック"} },
    { GenreIndex::GAME,       {"GAME", "ゲームミュージック"} },
    { GenreIndex::NAMCO,      {"NAMCO", "ナムコオリジナル"} },
    { GenreIndex::RECOMMENDED,{"RECOMMENDED"} },
    { GenreIndex::FAVORITE,   {"FAVORITE"} },
    { GenreIndex::RECENT,     {"RECENT"} },
    { GenreIndex::DAN,        {"DAN", "段位道場"} },
    { GenreIndex::DIFFICULTY, {"DIFFICULTY"} },
};

inline GenreIndex get_genre_index(const std::string& genreString) {
    std::string genreUpper = genreString;
    std::transform(genreUpper.begin(), genreUpper.end(), genreUpper.begin(), ::toupper);

    for (const auto& [genreIndex, genreSet] : GENRE_MAP) {
        if (genreSet.count(genreUpper)) {
            return genreIndex;
        }
    }
    return GenreIndex::DEFAULT;
}

const std::array<std::string, 6> COLLECTIONS = {
    "NEW", "RECENT", "FAVORITE", "DIFFICULTY", "RECOMMENDED", "SEARCH"
};
