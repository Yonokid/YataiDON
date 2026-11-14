#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <random>
#include <vector>

#include "../enums.h"

inline constexpr int BATTLE_BOND_MAX = 65;
inline constexpr int BATTLE_DEFAULT_BOND = 1;

inline constexpr std::array<std::array<int, 2>, 20> BATTLE_ATTACK_POINTS = {
    {{1, 30},   {2, 45},   {3, 60},   {4, 69},   {5, 81},   {6, 90},   {7, 99},   {12, 138}, {13, 144}, {14, 150},
     {15, 200}, {16, 223}, {17, 254}, {18, 270}, {19, 292}, {20, 316}, {21, 338}, {22, 361}, {23, 384}, {29, 572}}};

inline int battle_base_attack(int bond) {
    bond = std::clamp(bond, 1, BATTLE_BOND_MAX);
    for (size_t i = 1; i < BATTLE_ATTACK_POINTS.size(); ++i) {
        const auto [lv0, a0] = BATTLE_ATTACK_POINTS[i - 1];
        const auto [lv1, a1] = BATTLE_ATTACK_POINTS[i];
        if (bond <= lv1) {
            return a0 + (a1 - a0) * (bond - lv0) / (lv1 - lv0);
        }
    }
    const auto [lv0, a0] = BATTLE_ATTACK_POINTS[BATTLE_ATTACK_POINTS.size() - 2];
    const auto [lv1, a1] = BATTLE_ATTACK_POINTS.back();
    return a1 + (a1 - a0) * (bond - lv1) / (lv1 - lv0);
}

inline constexpr double BATTLE_CRITICAL_CHANCE = 0.1;
inline constexpr int BATTLE_ROLL_DIVISOR = 8;
inline constexpr int BATTLE_SKILL_ATTACKS = 20;
inline constexpr double BATTLE_CHEST_CHANCE = 0.1;
inline constexpr double BATTLE_ORB_CHEST_CHANCE = 0.1;
inline constexpr int BATTLE_MAX_CHESTS = 3;

inline constexpr int BATTLE_MOVE_COUNT = 10;
inline constexpr int BATTLE_MOVE_RANDOM = 10;
inline constexpr int BATTLE_MOVE_NONE = 11;
inline constexpr std::array<int, 9> BATTLE_MOVE_UNLOCKS = {0, 1, 2, 3, 5, 6, 7, 8, 9};
inline constexpr int BATTLE_MOVE_HACCHAKE = 4;
inline constexpr int BATTLE_SKILL_SLOTS = 3;

inline std::vector<int> battle_slot_items(int moves, bool hacchake) {
    const auto unlocked = static_cast<size_t>(std::clamp(moves, 1, static_cast<int>(BATTLE_MOVE_UNLOCKS.size())));
    std::vector<int> items(BATTLE_MOVE_UNLOCKS.begin(), BATTLE_MOVE_UNLOCKS.begin() + unlocked);
    if (hacchake) {
        items.insert(std::ranges::upper_bound(items, BATTLE_MOVE_HACCHAKE), BATTLE_MOVE_HACCHAKE);
    }
    items.push_back(BATTLE_MOVE_RANDOM);
    items.push_back(BATTLE_MOVE_NONE);
    return items;
}

inline int battle_slot_move(const std::vector<int>& slots, int moves, bool hacchake, int fired, std::mt19937& rng) {
    std::vector<int> set;
    std::ranges::copy_if(slots, std::back_inserter(set), [](int m) { return m != BATTLE_MOVE_NONE; });
    if (set.empty()) {
        return -1;
    }
    const int move = set[fired % set.size()];
    if (move != BATTLE_MOVE_RANDOM) {
        return move;
    }
    const std::vector<int> items = battle_slot_items(moves, hacchake);
    return items[std::uniform_int_distribution<size_t>(0, items.size() - 3)(rng)];
}

struct BattleHit {
    int damage = 0;
    bool critical = false;
};

// Good and OK land floor(A * r), r in [1, 1.2]; a Good crits for double about one time in ten.
inline BattleHit battle_note_damage(int attack, Judgments judgment, std::mt19937& rng) {
    if (judgment == Judgments::BAD) {
        return {};
    }
    std::uniform_real_distribution<double> spread(1.0, 1.2);
    BattleHit hit{static_cast<int>(std::floor(attack * spread(rng)))};
    if (judgment == Judgments::GOOD && std::uniform_real_distribution<double>(0.0, 1.0)(rng) < BATTLE_CRITICAL_CHANCE) {
        hit.damage *= 2;
        hit.critical = true;
    }
    return hit;
}

inline int battle_roll_damage(int attack, std::mt19937& rng) {
    const int hit = static_cast<int>(std::floor(attack * std::uniform_real_distribution<double>(1.0, 1.2)(rng)));
    return (hit + BATTLE_ROLL_DIVISOR - 1) / BATTLE_ROLL_DIVISOR;
}

inline int battle_skill_threshold(int difficulty, int fired) {
    static constexpr std::array<std::array<int, 8>, 4> TABLE = {{{30, 80, 160, 240, 320, 400, 480, 560},
                                                                 {50, 125, 235, 345, 455, 565, 675, 785},
                                                                 {75, 180, 330, 480, 630, 780, 930, 1080},
                                                                 {100, 250, 450, 650, 850, 1050, 1250, 1450}}};
    const auto& row = TABLE[std::clamp(difficulty, 0, 3)];
    if (fired < static_cast<int>(row.size())) {
        return row[fired];
    }
    return row.back() + (row.back() - row[row.size() - 2]) * (fired - static_cast<int>(row.size()) + 1);
}

inline float battle_skill_fill(int difficulty, int fired, int notes) {
    const int from = fired == 0 ? 0 : battle_skill_threshold(difficulty, fired - 1);
    const int to = battle_skill_threshold(difficulty, fired);
    return std::clamp(static_cast<float>(notes - from) / static_cast<float>(to - from), 0.0f, 1.0f);
}

struct BattleEnemySet {
    int kinds;
    int big;
    int bond;
};
inline constexpr std::array<BattleEnemySet, 5> BATTLE_ENEMIES = {
    {{5, 1, 1}, {5, 1, 14}, {8, 2, 29}, {7, 2, 41}, {6, 2, 53}}};

inline const BattleEnemySet& battle_enemy_set(int stage) {
    return BATTLE_ENEMIES[std::clamp((stage - 1) / 2, 0, static_cast<int>(BATTLE_ENEMIES.size()) - 1)];
}

inline int battle_enemy_hp(int stage, int kind) {
    const BattleEnemySet& set = battle_enemy_set(stage);
    return battle_base_attack(set.bond) * (kind >= set.kinds - set.big ? 2 : 1);
}

inline constexpr int BATTLE_FIELD_SIZE = 10;
inline constexpr int BATTLE_POWER_DAMAGE = 500;

inline constexpr int BATTLE_EX_STAGE = 11;

inline bool battle_is_boss_stage(int stage) {
    return stage % 2 == 0 || stage == BATTLE_EX_STAGE;
}

inline int battle_next_enemy(int stage, std::mt19937& rng) {
    return std::uniform_int_distribution<int>(0, battle_enemy_set(stage).kinds - 1)(rng);
}
