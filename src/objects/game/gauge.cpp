#include "gauge.h"

#include <cmath>
#include <stdexcept>

Gauge::Gauge(int total_notes, int difficulty, int level, PlayerNum player_num)
    : Gauge(GaugeCore(total_notes, difficulty, level), player_num) {}

Gauge::Gauge(const GaugeCore& core, PlayerNum player_num) : GaugeCore(core), player_num(player_num) {
    if (dan_mode) {
        string_diff = "";
    } else if (difficulty == static_cast<int>(Difficulty::EASY)) {
        string_diff = "_easy";
    } else if (difficulty <= static_cast<int>(Difficulty::HARD)) {
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
    if (songs.empty()) {
        throw std::runtime_error("Gauge::dan: songs is empty");
    }
    std::vector<std::pair<int, int>> courses;
    courses.reserve(songs.size());
    for (const DanSongEntry& s : songs) {
        courses.emplace_back(s.difficulty, s.level);
    }
    return Gauge(GaugeCore::dan(courses, total_notes), player_num);
}

void Gauge::add_good() {
    if (gauge_update_anim) {
        gauge_update_anim->start();
    }
    GaugeCore::add_good();
}

void Gauge::add_ok() {
    if (gauge_update_anim) {
        gauge_update_anim->start();
    }
    GaugeCore::add_ok();
}

void Gauge::add_bad() {
    GaugeCore::add_bad();

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
