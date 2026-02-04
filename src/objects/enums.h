#pragma once

#include <string>
#include <array>

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

inline std::string branch_diff_to_string(BranchDifficulty difficulty) {
    static const std::array<std::string, 3> names = {
        "normal",   // 0
        "expert",   // 1
        "master"    // 2
    };
    return names[static_cast<int>(difficulty)];
}
