#include "gauge.h"

#include <cmath>
#include <stdexcept>

Gauge::Gauge(int total_notes, int difficulty, int level, PlayerNum player_num) : player_num(player_num) {
    this->difficulty = std::clamp(difficulty, 0, static_cast<int>(Difficulty::ONI));
    clear_points = this->difficulty <= static_cast<int>(Difficulty::EASY)   ? 6000
                   : this->difficulty <= static_cast<int>(Difficulty::HARD) ? 7000
                                                                            : 8000;
    const GaugeTable& table_row = table[this->difficulty][std::clamp(level - 1, 0, 9)];
    const double denom = std::max(1, total_notes) * table_row.soul_percent;
    good_points = (denom > 0.0) ? 1'000'000.0 / denom : 0;
    ok_points = good_points * table_row.ok_multiplier;
    bad_points = good_points * table_row.bad_multiplier;
    points = 0;

    if (this->difficulty == static_cast<int>(Difficulty::EASY)) {
        string_diff = "_easy";
    } else if (this->difficulty <= static_cast<int>(Difficulty::HARD)) {
        string_diff = "_normal";
    } else {
        string_diff = "_hard";
    }

    tamashii_fire_change = dynamic_cast<TextureChangeAnimation*>(tex.get_animation(25, true));
    gauge_update_anim = dynamic_cast<FadeAnimation*>(tex.get_animation(10, true));
    if (!tamashii_fire_change || !gauge_update_anim) {
        throw std::runtime_error("Gauge: animation 25 or 10 is missing/has unexpected type");
    }
}

Gauge Gauge::dan(const std::vector<DanSongEntry>& songs, int total_notes, PlayerNum player_num) {
    // a missing LEVEL in the tja arrives as 0. treat it as oni 10 like player does
    auto diff_of = [](const DanSongEntry& s) {
        return s.level <= 0 ? static_cast<int>(Difficulty::ONI) : s.difficulty;
    };
    auto level_of = [](const DanSongEntry& s) { return s.level <= 0 ? 10 : s.level; };

    if (songs.empty()) {
        throw std::runtime_error("Gauge::dan: songs is empty");
    }
    const DanSongEntry& first = songs.at(0);
    Gauge g(total_notes, diff_of(first), level_of(first), player_num);
    g.dan_mode = true;
    g.string_diff = "";
    g.clear_points = g.max_points; // no norma zone, the course bar is full or it isn't

    // one rate for the whole course. harmonic mean of the songs soul percentages is what
    // the arcade does when it breaks the gauge into thirds. ok/bad just averaged
    GaugeTable row{0.0, 0.0, 0.0};
    double inv_sum = 0.0;
    for (const DanSongEntry& s : songs) {
        const int d = std::clamp(diff_of(s), 0, static_cast<int>(Difficulty::ONI));
        const GaugeTable& r = g.table[d][std::clamp(level_of(s) - 1, 0, 9)];
        if (r.soul_percent > 0.0) {
            inv_sum += 1.0 / r.soul_percent;
        }
        row.ok_multiplier += r.ok_multiplier / songs.size();
        row.bad_multiplier += r.bad_multiplier / songs.size();
    }
    row.soul_percent = songs.size() / inv_sum;

    const double denom = std::max(1, total_notes) * row.soul_percent;
    g.good_points = (denom > 0.0) ? 1'000'000.0 / denom : 0;
    g.ok_points = g.good_points * row.ok_multiplier;
    g.bad_points = g.good_points * row.bad_multiplier;
    return g;
}

void Gauge::apply_points_clamped(double delta) {
    previous_points = points;
    points = std::clamp<double>(points + delta, 0.0, max_points);
    if (std::abs(points - max_points) < POINTS_EPS) {
        points = max_points;
    }
    if (std::abs(points - clear_points) < POINTS_EPS) {
        points = clear_points;
    }
}

void Gauge::add_good() {
    if (gauge_update_anim) {
        gauge_update_anim->start();
    }
    apply_points_clamped(good_points);
}

void Gauge::add_ok() {
    if (gauge_update_anim) {
        gauge_update_anim->start();
    }
    apply_points_clamped(ok_points);
}

void Gauge::add_bad() {
    apply_points_clamped(bad_points);

    // this comparison is safe because apply_points_clamped snaps points onto max_points when within POINTS_EPS
    const bool was_full = previous_points >= max_points;
    if (was_full && points < max_points) {
        if (rainbow_fade_in.has_value() && rainbow_fade_in.value()) {
            rainbow_fade_in.value()->pause();
        }
        rainbow_fade_in.reset();
        rainbow_start_ms = -1.0;
        rainbow_frac = 0.0f;
    }
}

void Gauge::update(double current_ms) {
    if (get_is_rainbow() && !rainbow_fade_in.has_value()) {
        auto* anim = dynamic_cast<FadeAnimation*>(tex.get_animation(63));
        if (!anim) {
            throw std::runtime_error("Gauge: animation 63 has an unexpected type");
        }
        rainbow_fade_in = anim;
        rainbow_fade_in.value()->start();
        rainbow_start_ms = current_ms;
    }

    if (gauge_update_anim) {
        gauge_update_anim->update(current_ms);
    }
    if (tamashii_fire_change) {
        tamashii_fire_change->update(current_ms);
    }

    if (rainbow_fade_in.has_value()) {
        rainbow_fade_in.value()->update(current_ms);
        rainbow_frac = fmod((current_ms - rainbow_start_ms) / 75.0, 8.0);
    }
}

void Gauge::draw(float y) {
    if (!lua_tried) {
        lua_tried = true;
        if (load(script_class, script_file, player_num == PlayerNum::P2, dan_mode, string_diff)) {
            fn_draw = lua_object["draw"];
        } else {
            spdlog::error("Gauge: {}.lua failed to load, gauge will not be drawn", script_file);
        }
    }
    const bool anim_active = gauge_update_anim && gauge_update_anim->is_started && !gauge_update_anim->is_finished;
    const float anim_alpha = gauge_update_anim ? gauge_update_anim->attribute : 0.0f;
    const float rainbow_fade = rainbow_fade_in.has_value() ? rainbow_fade_in.value()->attribute : -1.0f;
    const int fire_frame = tamashii_fire_change ? static_cast<int>(tamashii_fire_change->attribute) : 0;
    call(fn_draw, "Gauge:draw", y, points, previous_points, max_points, clear_points, get_is_clear(), get_is_rainbow(),
         anim_active, anim_alpha, rainbow_fade, rainbow_frac, fire_frame);
}
