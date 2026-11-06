#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>
#include <random>
#include <vector>

#include "../enums.h"

inline constexpr int AI_COSTUME = 171;
inline constexpr int AI_TERRITORY_MAX = 9;
inline constexpr double AI_SHORT_SONG_MS = 100000.0;

struct AITally {
    int good = 0;
    int ok = 0;
    int bad = 0;
    int drumroll = 0;
};

inline int ai_section_count(double length_ms) {
    return length_ms >= AI_SHORT_SONG_MS ? 5 : 3;
}

inline int ai_territory_points(int course, const AITally& tally) {
    static constexpr int COEFFICIENTS[4][2] = {{5, 3}, {5, 3}, {3, 2}, {3, 1}};
    const int* coefficient = COEFFICIENTS[std::clamp(course, 0, 3)];
    return tally.good * coefficient[0] + tally.ok * coefficient[1];
}

inline void ai_count_judgment(AITally& tally, Judgments judgment) {
    ++(judgment == Judgments::GOOD ? tally.good : judgment == Judgments::OK ? tally.ok : tally.bad);
}

// Whoever has judged more notes waits for the other, so a note the AI has landed and the
// human has not yet reached never swings the territory. Judgments are in chart order.
inline int ai_territory_margin(int course, const std::vector<Judgments>& human, const std::vector<Judgments>& ai) {
    const size_t shared = std::min(human.size(), ai.size());
    AITally human_tally;
    AITally ai_tally;
    for (size_t i = 0; i < shared; ++i) {
        ai_count_judgment(human_tally, human[i]);
        ai_count_judgment(ai_tally, ai[i]);
    }
    return ai_territory_points(course, human_tally) - ai_territory_points(course, ai_tally);
}

inline std::vector<double> ai_section_starts(const std::vector<double>& note_ms, const std::vector<double>& barline_ms,
                                             int count) {
    std::vector<double> starts;
    if (note_ms.empty()) {
        return starts;
    }
    double previous = note_ms.front();
    for (int section = 1; section < count; ++section) {
        const double target = note_ms[note_ms.size() * section / count];
        double best = target;
        double best_distance = -1.0;
        for (const double bar : barline_ms) {
            const double distance = std::abs(bar - target);
            if (bar > previous && (best_distance < 0.0 || distance < best_distance)) {
                best = bar;
                best_distance = distance;
            }
        }
        starts.push_back(best);
        previous = best;
    }
    return starts;
}

inline int ai_section_of(const std::vector<double>& starts, double hit_ms) {
    static constexpr double ON_BAR_MS = 1.0;
    return static_cast<int>(std::distance(starts.begin(), std::ranges::upper_bound(starts, hit_ms + ON_BAR_MS)));
}

struct AIRates {
    double good = 1.0;
    double ok = 0.0;
};

struct AIBrain {
    static constexpr int LARGE_MARGIN = 5;
    static constexpr int MAX_ANGER = 2;
    static constexpr double ANGER_BOOST = 0.5;
    static constexpr int WEAK_AFTER_LOSSES = 2;
    static constexpr double WEAK_GOOD_FACTOR = 0.95;

    double condition = 0.95; // Good rate when it is left alone
    int anger = 0;
    int player_losses = 0;

    void on_section_end(int margin) {
        if (margin >= LARGE_MARGIN) {
            anger = std::min(anger + 1, MAX_ANGER);
        } else if (margin < 0) {
            anger = std::max(anger - 1, 0);
        }
        player_losses = margin < 0 ? player_losses + 1 : 0;
    }

    AIRates rates(const AITally& player) const {
        double good = condition;
        double ok = 1.0 - condition;
        if (player.good + player.ok > 0) {
            const double total = player.good + player.ok + player.bad;
            const double player_good = player.good / total;
            good = std::min(player_good, condition);
            ok = player.ok / total + (player_good - good);
        }
        const double boost = anger * ANGER_BOOST;
        const double rest = 1.0 - good;
        good += rest * boost;
        ok = rest > 0.0 ? ok / rest * (1.0 - good) : 0.0;
        if (player_losses >= WEAK_AFTER_LOSSES) {
            const double weakened = good * WEAK_GOOD_FACTOR;
            ok += good - weakened;
            good = weakened;
        }
        return {good, ok};
    }
};

inline Judgments ai_pick_judgment(const AIRates& rates, double roll) {
    if (roll < rates.good) {
        return Judgments::GOOD;
    }
    return roll < rates.good + rates.ok ? Judgments::OK : Judgments::BAD;
}

inline constexpr double AI_CONDITION_MIN = 0.90;
inline constexpr double AI_CONDITION_MAX = 0.99;

inline double ai_roll_condition() {
    std::mt19937 rng{std::random_device{}()};
    return std::uniform_real_distribution<double>(AI_CONDITION_MIN, AI_CONDITION_MAX)(rng);
}

inline int ai_condition_tier(double condition) {
    const double share = (condition - AI_CONDITION_MIN) / (AI_CONDITION_MAX - AI_CONDITION_MIN);
    return std::clamp(static_cast<int>(share * 5), 0, 4);
}
