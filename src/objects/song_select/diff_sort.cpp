#include "diff_sort.h"

#include "../../libs/audio/audio.h"
#include "song_select_script.h"

static constexpr double SORT_IN_MS = 1000.0 / 60.0 * 10.0;
static constexpr double SORT_OUT_MS = 1000.0 / 60.0 * 10.0;
static constexpr double SORT_START_WAIT_MS = 1000.0;
static constexpr double SORT_END_WAIT_MS = 2000.0;
static constexpr double SORT_ARROW_MS = 1000.0 / 60.0 * 10.0;
static constexpr float SORT_ARROW_PUSH = 12.5f;

DiffSortSelect::DiffSortSelect(const Statistics& statistics, int prev_diff, int prev_level, SongSelectScript* script,
                               int prev_order, bool two_players)
    : prev_diff(prev_diff), prev_level(prev_level), statistics(statistics), script(script) {
    one_menu_sort = tex.options[SCO::ONE_MENU_SORT] && script && script->has_sort_window();
    selected_box = -1;
    selected_level = 1;
    in_level_select = false;
    confirmation = false;
    confirm_index = 1;
    num_boxes = 6;
    limits = {5, 7, 8, 10, 10};

    bg_resize = dynamic_cast<TextureResizeAnimation*>(tex.get_animation(19));
    diff_fade_in = dynamic_cast<FadeAnimation*>(tex.get_animation(20));
    box_flicker = dynamic_cast<FadeAnimation*>(tex.get_animation(21));
    confirmation_bounce = dynamic_cast<MoveAnimation*>(tex.get_animation(22));
    blue_arrow_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(29, true));
    blue_arrow_move = dynamic_cast<MoveAnimation*>(tex.get_animation(30, true));

    if (!one_menu_sort) {
        bg_resize->start();
        diff_fade_in->start();
        box_flicker->start();
    }

    compute_sums();

    if (one_menu_sort) {
        sort_param[0] = (prev_diff >= 0 && prev_diff < diff_count) ? prev_diff + 1 : 1;
        sort_param[1] = (prev_level >= star_min && prev_level <= star_max) ? prev_level : 1;
        sort_param[2] = (prev_order >= 1 && prev_order <= order_count) ? prev_order : 1;
        session = 0;
        t_open = now_ms = get_current_ms();
        one_menu_sort_refresh_song_num();
    }

    which_dialog = two_players && !one_menu_sort;
    audio.play_sound(which_dialog ? "voice_diff_sort_which" : "voice_diff_sort_enter", VolumePreset::VOICE);
}

void DiffSortSelect::compute_sums() {
    for (const auto& [course, levels] : statistics) {
        std::array<int, 3> sums = {0, 0, 0};
        for (const auto& [level, stats] : levels) {
            sums[0] += stats.total;
            sums[1] += stats_player == 1 ? stats.clears_2p : stats.clears;
            sums[2] += stats_player == 1 ? stats.full_combos_2p : stats.full_combos;
        }
        diff_sort_sum_stat[course] = sums;
    }
}

void DiffSortSelect::one_menu_sort_refresh_song_num() {
    song_num = 0;
    auto course_it = statistics.find(std::min(sort_param[0], 5) - 1);
    if (course_it == statistics.end()) {
        return;
    }
    auto level_it = course_it->second.find(sort_param[1]);
    if (level_it == course_it->second.end()) {
        return;
    }
    song_num = level_it->second.total;
}

bool DiffSortSelect::one_menu_sort_input_locked() const {
    return (now_ms - t_open) < SORT_START_WAIT_MS || t_end > 0.0;
}

int DiffSortSelect::lua_phase() const {
    if (t_out > 0.0) {
        return 3;
    }
    if (t_end > 0.0) {
        return 2;
    }
    if ((now_ms - t_open) < SORT_IN_MS) {
        return 0;
    }
    return 1;
}

float DiffSortSelect::lua_alpha() const {
    if (t_out > 0.0) {
        float u = (now_ms - t_out) / SORT_OUT_MS;
        return std::max(0.0f, 1.0f - u);
    }
    float u = (now_ms - t_open) / SORT_IN_MS;
    return std::min(1.0f, std::max(0.0f, u));
}

float DiffSortSelect::lua_arrow_offset(int row) const {
    if (row < 0 || row > 2 || arrow_dir[row] == 0) {
        return 0.0f;
    }
    double e = now_ms - arrow_t0[row];
    if (e < 0.0 || e >= SORT_ARROW_MS) {
        return 0.0f;
    }
    double f = e / SORT_ARROW_MS * 10.0;
    float local = (f <= 5.0) ? (-SORT_ARROW_PUSH * (f / 5.0)) : (-SORT_ARROW_PUSH * (1.0 - (f - 5.0) / 5.0));
    return local * arrow_dir[row];
}

void DiffSortSelect::one_menu_sort_change_param(int move) {
    if (one_menu_sort_input_locked()) {
        return;
    }
    audio.play_sound("kat", VolumePreset::SOUND);
    int& v = sort_param[session];
    v += move;
    if (session == 1) {
        v = std::min(star_max, std::max(star_min, v));
    } else if (session == 0) {
        v = std::min(diff_count, std::max(1, v));
    } else {
        v = std::min(order_count, std::max(1, v));
    }
    one_menu_sort_refresh_song_num();
    arrow_dir[session] = (move > 0) ? 1 : -1;
    arrow_t0[session] = now_ms;
}

std::optional<std::array<int, 3>> DiffSortSelect::take_result() {
    if (!finished) {
        return std::nullopt;
    }
    finished = false;
    return one_menu_sort_result;
}

void DiffSortSelect::update(double current_ms) {
    if (one_menu_sort) {
        now_ms = current_ms;
        if (t_end > 0.0 && t_out == 0.0 && (current_ms - t_end) >= SORT_END_WAIT_MS) {
            t_out = current_ms; // CloseSortWindow -> label "out"
        }
        if (t_out > 0.0 && !one_menu_sort_result && (current_ms - t_out) >= SORT_OUT_MS) {
            one_menu_sort_result = std::array<int, 3>{sort_param[0] - 1, sort_param[1], sort_param[2]};
            finished = true;
        }
        return;
    }
    bg_resize->update(current_ms);
    diff_fade_in->update(current_ms);
    box_flicker->update(current_ms);
    confirmation_bounce->update(current_ms);
    blue_arrow_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
}

std::optional<std::pair<int, int>> DiffSortSelect::input_select() {
    if (one_menu_sort) {
        if (one_menu_sort_input_locked()) {
            return std::nullopt;
        }
        if (session == 1 && song_num <= 0) {
            audio.play_sound("cancel", VolumePreset::SOUND);
            return std::nullopt;
        }
        audio.play_sound("don", VolumePreset::SOUND);
        session++;
        if (session > 2) {
            t_end = now_ms;
        }
        return std::nullopt;
    }
    if (which_dialog) {
        if (which_index == 1) {
            return {{-1, -1}};
        }
        which_dialog = false;
        stats_player = which_index / 2;
        compute_sums();
        bg_resize->start();
        diff_fade_in->start();
        box_flicker->start();
        audio.play_sound("voice_diff_sort_enter", VolumePreset::VOICE);
        return std::nullopt;
    }
    if (confirmation) {
        if (confirm_index == 0) {
            confirmation = false;
            return std::nullopt;
        }

        if (confirm_index == 1) {
            return {{selected_box, selected_level}};
        }

        if (confirm_index == 2) {
            confirmation = false;
            in_level_select = false;
            return std::nullopt;
        }
    } else if (in_level_select) {
        auto course = statistics.find(selected_box);
        if (course == statistics.end() || course->second[selected_level].total == 0) {
            audio.play_sound("cancel", VolumePreset::SOUND);
            return std::nullopt;
        }
        confirmation = true;
        confirmation_bounce->start();
        confirm_index = 1;
        audio.play_sound("voice_diff_sort_confirm", VolumePreset::VOICE);
        return std::nullopt;
    }
    if (selected_box == -1) {
        return {{-1, -1}};
    }
    if (selected_box == 5) {
        return {{prev_diff, prev_level}};
    }

    audio.play_sound("voice_diff_sort_level", VolumePreset::VOICE);
    in_level_select = true;
    bg_resize->start();
    diff_fade_in->start();
    selected_level = std::min(selected_level, limits[selected_box]);
    return std::nullopt;
}

void DiffSortSelect::input_left() {
    if (one_menu_sort) {
        one_menu_sort_change_param(-1);
        return;
    }
    if (which_dialog) {
        which_index = std::max(which_index - 1, 0);
    } else if (confirmation) {
        confirm_index = std::max(confirm_index - 1, 0);
    } else if (in_level_select) {
        selected_level = std::max(selected_level - 1, 1);
    } else {
        selected_box = std::max(selected_box - 1, -1);
    }
}

void DiffSortSelect::input_right() {
    if (one_menu_sort) {
        one_menu_sort_change_param(1);
        return;
    }
    if (which_dialog) {
        which_index = std::min(which_index + 1, 2);
    } else if (confirmation) {
        confirm_index = std::min(confirm_index + 1, 2);
    } else if (in_level_select) {
        selected_level = std::min(selected_level + 1, limits[selected_box]);
    } else {
        selected_box = std::min(selected_box + 1, num_boxes - 1);
    }
}

void DiffSortSelect::draw() {
    if (script) {
        script->draw_sort_window(this);
    }
}
