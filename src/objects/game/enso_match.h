#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

#include "../enums.h"

inline constexpr int ENSO_BOND_MAX = 65;
// stand-in until Hiroba keeps the player's bond level
inline constexpr int ENSO_DEFAULT_BOND = 1;

inline constexpr std::array<std::array<int, 2>, 20> ENSO_ATTACK_POINTS = {
    {{1, 30},   {2, 45},   {3, 60},   {4, 69},   {5, 81},   {6, 90},   {7, 99},   {12, 138}, {13, 144}, {14, 150},
     {15, 200}, {16, 223}, {17, 254}, {18, 270}, {19, 292}, {20, 316}, {21, 338}, {22, 361}, {23, 384}, {29, 572}}};

inline int enso_base_attack(int bond) {
    bond = std::clamp(bond, 1, ENSO_BOND_MAX);
    for (size_t i = 1; i < ENSO_ATTACK_POINTS.size(); ++i) {
        const auto [lv0, a0] = ENSO_ATTACK_POINTS[i - 1];
        const auto [lv1, a1] = ENSO_ATTACK_POINTS[i];
        if (bond <= lv1) {
            return a0 + (a1 - a0) * (bond - lv0) / (lv1 - lv0);
        }
    }
    const auto [lv0, a0] = ENSO_ATTACK_POINTS[ENSO_ATTACK_POINTS.size() - 2];
    const auto [lv1, a1] = ENSO_ATTACK_POINTS.back();
    return a1 + (a1 - a0) * (bond - lv1) / (lv1 - lv0);
}

inline constexpr double ENSO_CRITICAL_CHANCE = 0.1;
inline constexpr int ENSO_ROLL_DIVISOR = 8;
// unknown in the source notes: a special move hits for this many base attacks
inline constexpr int ENSO_SKILL_ATTACKS = 20;
// unknown: chance that a fired special move drops a treasure chest
inline constexpr double ENSO_CHEST_CHANCE = 0.5;

struct EnsoHit {
    int damage = 0;
    bool critical = false;
};

// Good and OK land floor(A * r), r in [1, 1.2]; a Good crits for double about one time in ten.
inline EnsoHit enso_note_damage(int attack, Judgments judgment, std::mt19937& rng) {
    if (judgment == Judgments::BAD) {
        return {};
    }
    std::uniform_real_distribution<double> spread(1.0, 1.2);
    EnsoHit hit{static_cast<int>(std::floor(attack * spread(rng)))};
    if (judgment == Judgments::GOOD && std::uniform_real_distribution<double>(0.0, 1.0)(rng) < ENSO_CRITICAL_CHANCE) {
        hit.damage *= 2;
        hit.critical = true;
    }
    return hit;
}

inline int enso_roll_damage(int attack, std::mt19937& rng) {
    const int hit = static_cast<int>(std::floor(attack * std::uniform_real_distribution<double>(1.0, 1.2)(rng)));
    return (hit + ENSO_ROLL_DIVISOR - 1) / ENSO_ROLL_DIVISOR;
}

inline int enso_skill_threshold(int difficulty, int fired) {
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

// Fill of the special-move gauge, 0..1, between the previous and the next threshold.
inline float enso_skill_fill(int difficulty, int fired, int notes) {
    const int from = fired == 0 ? 0 : enso_skill_threshold(difficulty, fired - 1);
    const int to = enso_skill_threshold(difficulty, fired);
    return std::clamp(static_cast<float>(notes - from) / static_cast<float>(to - from), 0.0f, 1.0f);
}

// Stage 1's five minor enemies (sprout, moth, turtle, chameleon, wolf). Guessed from footage: one hit at bond 1 fells a
// small one, a critical fells two, and the wolf takes two hits.
inline constexpr std::array<int, 5> ENSO_ZAKO_HP = {30, 30, 30, 30, 60};
inline constexpr int ENSO_FIELD_SIZE = 10;
// guessed from footage: battle power climbs about one per 500 damage
inline constexpr int ENSO_POWER_DAMAGE = 500;

inline int enso_next_zako(std::mt19937& rng) {
    return std::uniform_int_distribution<int>(0, static_cast<int>(ENSO_ZAKO_HP.size()) - 1)(rng);
}
