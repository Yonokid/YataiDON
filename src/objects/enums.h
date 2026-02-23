#pragma once

#include <string>
#include <array>

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


