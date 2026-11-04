#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <map>
#include <set>
#include <string>

enum class GaugeMode {
    NORMAL,
    DAN
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
    SEARCHING = 3,
    DAN_SELECTED = 4,
    TUTORIAL_SELECTED = 5,
    NORMAL_MODE_SELECTED = 6,
    WAIWAI_TUTORIAL_SELECTED = 7,
    WAIWAI_MODE_SELECTED = 8
};

inline std::string branch_diff_to_string(BranchDifficulty difficulty) {
    static const std::array<std::string, 3> names = {
        "normal", // 0
        "expert", // 1
        "master"  // 2
    };
    int index = static_cast<int>(difficulty);
    if (index < 0 || index >= std::ssize(names)) {
        return names[0];
    }
    return names[index];
}

enum class TextureIndex : int {
    NONE = 0,
    VOCALOID = 1,
    DEFAULT = 2,
    RECOMMENDED = 3,
    FAVORITE = 4,
    RECENT = 5
};

// Chart inside a GENRE:TUTORIAL folder (box.def); song select plays it as the tutorial
inline constexpr const char* TUTORIAL_CHART = "Tutorial.tja";

enum class GenreIndex : int {
    TUTORIAL = 0,
    JPOP = 1,
    ANIME = 2,
    VOCALOID = 3,
    CHILDREN = 4,
    VARIETY = 5,
    CLASSICAL = 6,
    GAME = 7,
    NAMCO = 8,
    DEFAULT = 9,
    RECOMMENDED = 10,
    FAVORITE = 11,
    RECENT = 12,
    DAN = 13,
    DIFFICULTY = 14
};

inline const std::map<GenreIndex, int> GENRE_TO_REF_FRAME = {
    {GenreIndex::TUTORIAL, 0}, {GenreIndex::JPOP, 1},    {GenreIndex::ANIME, 2},       {GenreIndex::VOCALOID, 8},
    {GenreIndex::CHILDREN, 7}, {GenreIndex::VARIETY, 6}, {GenreIndex::CLASSICAL, 5},   {GenreIndex::GAME, 3},
    {GenreIndex::NAMCO, 4},    {GenreIndex::DEFAULT, 0}, {GenreIndex::RECOMMENDED, 0}, {GenreIndex::FAVORITE, 0},
    {GenreIndex::RECENT, 9},   {GenreIndex::DAN, 0},     {GenreIndex::DIFFICULTY, 0},
};

inline int genre_to_ref_frame(GenreIndex genre) {
    auto it = GENRE_TO_REF_FRAME.find(genre);
    return it != GENRE_TO_REF_FRAME.end() ? it->second : 0;
}

inline const std::map<std::string, TextureIndex> TEXTURE_MAP = {
    {"VOCALOID", TextureIndex::VOCALOID},       {"ボーカロイド", TextureIndex::VOCALOID},
    {"RECOMMENDED", TextureIndex::RECOMMENDED}, {"FAVORITE", TextureIndex::FAVORITE},
    {"RECENT", TextureIndex::RECENT},
};

inline const std::map<GenreIndex, std::set<std::string>> GENRE_MAP = {
    {GenreIndex::TUTORIAL, {"TUTORIAL"}},
    {GenreIndex::JPOP, {"J-POP"}},
    {GenreIndex::ANIME, {"ANIME", "アニメ"}},
    {GenreIndex::CHILDREN, {"CHILDREN", "どうよう"}},
    {GenreIndex::VOCALOID, {"VOCALOID", "ボーカロイド"}},
    {GenreIndex::VARIETY, {"VARIETY", "バラエティー", "バラエティ"}},
    {GenreIndex::CLASSICAL, {"CLASSICAL", "クラシック"}},
    {GenreIndex::GAME, {"GAME", "ゲームミュージック"}},
    {GenreIndex::NAMCO, {"NAMCO", "ナムコオリジナル"}},
    {GenreIndex::RECOMMENDED, {"RECOMMENDED"}},
    {GenreIndex::FAVORITE, {"FAVORITE"}},
    {GenreIndex::RECENT, {"RECENT"}},
    {GenreIndex::DAN, {"DAN", "段位道場"}},
    {GenreIndex::DIFFICULTY, {"DIFFICULTY"}},
};

inline GenreIndex get_genre_index(const std::string& genreString) {
    std::string genreUpper = genreString;
    std::ranges::transform(genreUpper, genreUpper.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    for (const auto& [genreIndex, genreSet] : GENRE_MAP) {
        if (genreSet.contains(genreUpper)) {
            return genreIndex;
        }
    }
    return GenreIndex::DEFAULT;
}

// Boxes the game adds. On the waiwai screen: back to the normal 2P select, and the how-to-play movie.
inline const std::string WAIWAI_NORMAL_COLLECTION = "WAIWAI_NORMAL";
inline const std::string WAIWAI_TUTORIAL_COLLECTION = "WAIWAI_TUTORIAL";
// On the normal 2P select: into waiwai, for pairs where someone is logged in.
inline const std::string WAIWAI_MODE_COLLECTION = "WAIWAI_MODE";

inline const std::array<std::string, 6> COLLECTIONS = {"NEW",        "RECENT",      "FAVORITE",
                                                       "DIFFICULTY", "RECOMMENDED", "SEARCH"};
