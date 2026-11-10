#include "judge.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <ranges>
#include <spdlog/spdlog.h>
#include <sstream>
#include <utility>

void compute_load_time(Note& note, ScrollType scroll_type, const TempoMap& tempo_map) {
    const int note_half_w = Lane::NOTE_HALF_W;
    const float travel_distance = Lane::TRAVEL_DISTANCE;
    bool horizontal = std::abs(note.scroll_x) >= std::abs(note.scroll_y);
    float base_pixels_per_ms =
        (note.bpm / 240000 * (horizontal ? std::abs(note.scroll_x) : std::abs(note.scroll_y)) * travel_distance);
    if (base_pixels_per_ms == 0) {
        note.load_ms = note.hit_ms;
        note.unload_ms = note.hit_ms;
        return;
    }
    float normal_travel_ms = (travel_distance + note_half_w) / base_pixels_per_ms;

    if (!note.sudden_appear_ms.has_value() || !note.sudden_moving_ms.has_value() ||
        note.sudden_appear_ms.value() == std::numeric_limits<float>::infinity()) {
        if (scroll_type != ScrollType::NMSCROLL) {
            double px_per_beat = (horizontal ? note.scroll_x : std::abs(note.scroll_y)) * travel_distance / 4;
            double left = horizontal ? -(Lane::JUDGE_X + note_half_w) : -(travel_distance + note_half_w);
            double right = travel_distance + note_half_w;
            double beat = tempo_map.beat_at(note.hit_ms);
            double lo = beat - right / px_per_beat, hi = beat - left / px_per_beat;
            auto [first, last] = tempo_map.ms_within(std::min(lo, hi), std::max(lo, hi));
            note.load_ms = std::min(first, note.hit_ms);
            note.unload_ms = std::max(last, note.hit_ms);
            return;
        }
        note.load_ms = note.hit_ms - normal_travel_ms;
        note.unload_ms = note.hit_ms + normal_travel_ms;
        return;
    }
    note.load_ms = note.hit_ms - note.sudden_appear_ms.value();
    float movement_duration = note.sudden_moving_ms.value();
    if (movement_duration <= 0) {
        movement_duration = normal_travel_ms;
    }
    float sudden_pixels_per_ms = travel_distance / movement_duration;
    float unload_offset = travel_distance / sudden_pixels_per_ms;
    note.unload_ms = note.hit_ms + unload_offset;
}

bool note_unplayable(const Note& note, ScrollType scroll_type, std::optional<double> audio_end_ms) {
    return scroll_type != ScrollType::NMSCROLL && audio_end_ms.has_value() && note.type != NoteType::BARLINE &&
           note.hit_ms > audio_end_ms.value();
}

void prepare_chart(NoteList& notes, std::deque<NoteList>& branch_m, std::deque<NoteList>& branch_e,
                   std::deque<NoteList>& branch_n, const Modifiers& modifiers, std::optional<double> audio_end_ms) {
    apply_modifiers(notes, modifiers);
    Note* last_note = nullptr;
    for (Note& note : notes.notes) {
        compute_load_time(note, notes.scroll_type, notes.tempo_map);
        if (note.type == NoteType::TAIL && last_note != nullptr) {
            note.load_ms = last_note->load_ms;
            last_note->unload_ms = note.unload_ms;
        }
        if (note.type != NoteType::BARLINE) {
            last_note = &note;
        }
    }

    for (auto* branch : {&branch_m, &branch_e, &branch_n}) {
        for (NoteList& section : *branch) {
            apply_modifiers(section, modifiers);
            std::erase_if(section.notes,
                          [&](const Note& n) { return note_unplayable(n, notes.scroll_type, audio_end_ms); });
            Note* section_last = nullptr;
            for (Note& note : section.notes) {
                compute_load_time(note, notes.scroll_type, notes.tempo_map);
                if (note.type == NoteType::TAIL && section_last != nullptr) {
                    note.load_ms = section_last->load_ms;
                    section_last->unload_ms = note.unload_ms;
                }
                section_last = &note;
            }
        }
    }
}

bool is_judge_timeline_entry(const TimelineObject& entry) {
    if (entry.bpm.has_value() || (entry.judge_pos_x.has_value() && entry.judge_pos_y.has_value()) ||
        entry.gogo_time.has_value()) {
        return false;
    }
    if (entry.branch_params.has_value()) {
        return true;
    }
    if (entry.lyric.has_value() || entry.cam.has_value()) {
        return false;
    }
    return entry.section_reset.has_value();
}

namespace {
    using Row = std::array<double, 3>; // soul_percent, ok_multiplier, bad_multiplier
    // clang-format off
    constexpr std::array<std::array<Row, 10>, 4> GAUGE_TABLE = {{
        // Easy (★1–5, ★6–10 unused)
        {{{60.0, 0.75, -0.5}, {63.333, 0.75, -0.5}, {63.333, 0.75, -0.5}, {73.333, 0.75, -0.5}, {73.333, 0.75, -0.5},
          {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}},
        // Normal (★1–7, ★8–10 unused; ★1–3 and ★6–7 ok/bad assumed)
        {{{65.6, 0.75, -0.5}, {65.6, 0.75, -0.5}, {69.5, 0.75, -0.5}, {70.3, 0.75, -0.75}, {75.0, 0.75, -1.0},
          {75.0, 0.75, -1.0}, {75.0, 0.75, -1.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}},
        // Hard (★1–8, ★9–10 unused; ok mostly assumed)
        {{{77.6, 0.75, -0.75}, {77.6, 0.75, -0.75}, {72.5, 0.75, -1.0}, {69.15, 0.75, -1.17}, {67.5, 0.75, -1.25},
          {68.74, 0.75, -1.25}, {68.74, 0.75, -1.25}, {68.74, 0.75, -1.25}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}},
        // Oni (★1–10)
        {{{70.75, 0.5, -1.6}, {70.75, 0.5, -1.6}, {70.75, 0.5, -1.6}, {70.75, 0.5, -1.6}, {70.75, 0.5, -1.6},
          {70.75, 0.5, -1.6}, {70.75, 0.5, -1.6}, {70.0, 0.5, -2.0}, {76.75, 0.5, -2.0}, {76.75, 0.5, -2.0}}},
    }};
    // clang-format on
} // namespace

const GaugeCore::GaugeTable& GaugeCore::table_row(int difficulty, int level) {
    static const auto rows = [] {
        std::array<std::array<GaugeTable, 10>, 4> out{};
        for (size_t d = 0; d < 4; ++d) {
            for (size_t l = 0; l < 10; ++l) {
                out[d][l] = {GAUGE_TABLE[d][l][0], GAUGE_TABLE[d][l][1], GAUGE_TABLE[d][l][2]};
            }
        }
        return out;
    }();
    return rows[std::clamp(difficulty, 0, 3)][std::clamp(level - 1, 0, 9)];
}

GaugeCore::GaugeCore(int total_notes, int difficulty_param, int level) {
    difficulty = std::clamp(difficulty_param, 0, 3);
    clear_points = difficulty <= 0 ? 6000 : difficulty <= 2 ? 7000 : 8000;
    const GaugeTable& row = table_row(difficulty, level);
    const double denom = std::max(1, total_notes) * row.soul_percent;
    good_points = (denom > 0.0) ? 1'000'000.0 / denom : 0;
    ok_points = good_points * row.ok_multiplier;
    bad_points = good_points * row.bad_multiplier;
    points = 0;
}

GaugeCore GaugeCore::dan(const std::vector<std::pair<int, int>>& songs, int total_notes) {
    auto diff_of = [](const std::pair<int, int>& s) { return s.second <= 0 ? 3 : s.first; };
    auto level_of = [](const std::pair<int, int>& s) { return s.second <= 0 ? 10 : s.second; };

    GaugeCore g(total_notes, diff_of(songs.at(0)), level_of(songs.at(0)));
    g.dan_mode = true;
    g.clear_points = g.max_points;

    GaugeTable row{0.0, 0.0, 0.0};
    double inv_sum = 0.0;
    for (const auto& s : songs) {
        const GaugeTable& r = table_row(diff_of(s), level_of(s));
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

void GaugeCore::apply_points_clamped(double delta) {
    previous_points = points;
    points = std::clamp<double>(points + delta, 0.0, max_points);
    if (std::abs(points - max_points) < POINTS_EPS) {
        points = max_points;
    }
    if (std::abs(points - clear_points) < POINTS_EPS) {
        points = clear_points;
    }
}

JudgeCore::JudgeCore(int difficulty, Modifiers modifiers, bool gen3, bool is_2p)
    : is_2p(is_2p), difficulty(difficulty), modifiers(std::move(modifiers)), gen3(gen3) {}

void JudgeCore::load_chart(const NoteList& notes, std::deque<NoteList> branch_m_param,
                           std::deque<NoteList> branch_e_param, std::deque<NoteList> branch_n_param,
                           std::map<int, CourseData>& course_data, std::optional<double> audio_end_ms) {
    don_notes.clear();
    kat_notes.clear();
    other_notes.clear();
    chart_end_ms = 0;
    for (const Note& note : notes.notes) {
        if (note_unplayable(note, notes.scroll_type, audio_end_ms)) {
            continue;
        }
        if (note.type == NoteType::DON || note.type == NoteType::DON_L) {
            don_notes.push_back(note);
        } else if (note.type == NoteType::KAT || note.type == NoteType::KAT_L) {
            kat_notes.push_back(note);
        } else if (note.type != NoteType::BARLINE) {
            other_notes.push_back(note);
        }
        chart_end_ms = std::max(chart_end_ms, note.hit_ms);
    }
    branch_m = std::move(branch_m_param);
    branch_e = std::move(branch_e_param);
    branch_n = std::move(branch_n_param);
    for (const auto* branch : {&branch_m, &branch_e, &branch_n}) {
        for (const NoteList& section : *branch) {
            for (const Note& note : section.notes) {
                chart_end_ms = std::max(chart_end_ms, note.hit_ms);
            }
        }
    }

    judge_timeline.clear();
    std::ranges::copy_if(notes.timeline, std::back_inserter(judge_timeline), is_judge_timeline_entry);
    std::ranges::stable_sort(judge_timeline, {}, &TimelineObject::start_time);

    is_drumroll = false;
    curr_drumroll_count = 0;
    is_balloon = false;
    curr_balloon_count = 0;
    kusudama_shared_hits = 0;
    kusudama_key = -1;
    is_branch = false;
    branch_condition = "";
    branch_p_count = 0;
    branch_r_count = 0;
    branch_note_count = 0;
    branch_checkpoint_index = 0;

    NoteList total_notes; // all notes including master branch
    std::ranges::copy_if(notes.notes, std::back_inserter(total_notes.notes),
                         [&](const Note& n) { return !note_unplayable(n, notes.scroll_type, audio_end_ms); });
    for (const NoteList& section : branch_m) {
        total_notes.notes.insert(total_notes.notes.end(), section.notes.begin(), section.notes.end());
    }

    int stars = course_data.empty() ? 10 : static_cast<int>(course_data[difficulty].level);
    if (stars == 0) {
        difficulty = 3;
        stars = 10;
    }
    int gauge_total_notes = 0;
    for (const Note& note : total_notes.notes) {
        if (note.type >= NoteType::DON && note.type <= NoteType::KAT_L) {
            gauge_total_notes++;
        }
    }
    judgeable_note_count = gauge_total_notes;
    gauge_stars = stars;
    judge_gauge.emplace(gauge_total_notes, difficulty, stars);

    base_score = 0;
    score_init = 0;
    score_diff = 0;
    if (!gen3) {
        base_score = calculate_base_score(total_notes);
        return;
    }
    if (difficulty < 0 || difficulty >= std::ssize(course_data)) {
        score_init = calculate_base_score(total_notes);
        return;
    }
    score_diff = static_cast<int>(course_data[difficulty].scorediff);
    if (score_diff <= 0) {
        spdlog::warn("Error: No scorediff specified or scorediff less than 0 | Using shinuchi scoring method instead");
        score_diff = 0;
    }
    const std::vector<int>& score_init_list = course_data[difficulty].scoreinit;
    if (score_init_list.empty()) {
        spdlog::warn("Error: No scoreinit specified or scoreinit less than 0 | Using shinuchi scoring method instead");
        score_init = calculate_base_score(total_notes);
        score_diff = 0;
    } else {
        score_init = score_init_list[0];
    }
}

void JudgeCore::begin_frame(double ms_from_start) {
    play_note_manager(ms_from_start);
    handle_judge_timeline(ms_from_start);
}

void JudgeCore::end_frame(double ms_from_start) {
    if (is_branch) {
        evaluate_branch(ms_from_start);
    }
}

JudgeResult JudgeCore::result() const {
    JudgeResult r;
    r.score = score;
    r.good = good_count;
    r.ok = ok_count;
    r.bad = bad_count;
    r.max_combo = max_combo;
    r.drumroll = total_drumroll;
    if (judge_gauge.has_value()) {
        r.cleared = judge_gauge->get_is_clear();
        r.rainbow = judge_gauge->get_is_rainbow();
    }
    return r;
}

void JudgeCore::handle_judge_timeline(double ms_from_start) {
    // everything due this frame, newest first (the order Player's timeline buffer handled them in)
    std::vector<TimelineObject> due;
    while (!judge_timeline.empty() && ms_from_start > judge_timeline.front().start_time) {
        due.push_back(judge_timeline.front());
        judge_timeline.pop_front();
    }
    for (auto& it : std::views::reverse(due)) {
        handle_judge_entry(ms_from_start, it);
    }
}

void JudgeCore::handle_judge_entry(double ms_from_start, const TimelineObject& entry) {
    if (!entry.branch_params.has_value()) { // #SECTION
        branch_p_count = 0;
        branch_r_count = 0;
        branch_note_count = 0;
        return;
    }

    std::vector<std::string> parts;
    std::stringstream ss(entry.branch_params.value());
    std::string part;
    while (std::getline(ss, part, ',')) {
        parts.push_back(part);
    }
    if (parts.size() < 3 || is_branch) {
        return;
    }

    float e_req = 0.0f, m_req = 0.0f;
    try {
        e_req = std::stof(parts[1]);
        m_req = std::stof(parts[2]);
    } catch (const std::exception& e) {
        spdlog::warn("Invalid #BRANCHSTART params '{}': {}", entry.branch_params.value(), e.what());
        e_req = m_req = 0.0f;
    }

    is_branch = true;
    branch_condition = parts[0];

    // decided when the next section's first note comes on screen
    double branch_condition_end_time = ms_from_start;
    for (const auto* branch : {&branch_m, &branch_e, &branch_n}) {
        if (!branch->empty() && !branch->front().notes.empty()) {
            branch_condition_end_time = branch->front().notes.front().load_ms;
            break;
        }
    }
    if (branch_condition == "r" || branch_condition == "p") {
        curr_branch_reqs = std::make_tuple(e_req, m_req, branch_condition_end_time);
    }
    spdlog::debug("branch condition {} {} {} from {} until {}", branch_condition, e_req, m_req, entry.start_time,
                  branch_condition_end_time);
}

void JudgeCore::seek_judge(double resume_time) {
    resume_filter_ms = resume_time;
    while (!judge_timeline.empty() && judge_timeline.front().start_time <= resume_time) {
        TimelineObject entry = judge_timeline.front();
        judge_timeline.pop_front();
        handle_judge_entry(resume_time, entry);
    }
    const double boundary_eps = 1.0;
    auto filter = [resume_time, boundary_eps](std::deque<Note>& q) {
        while (!q.empty() && q.front().hit_ms < resume_time - boundary_eps) {
            q.pop_front();
        }
    };
    filter(don_notes);
    filter(kat_notes);
    filter(other_notes);
}

void JudgeCore::merge_branch_section(const NoteList& branch_section, double current_ms) {
    const double boundary_eps = 1.0;
    std::deque<Note> notes;
    for (const Note& note : branch_section.notes) {
        if (note.hit_ms >= resume_filter_ms - boundary_eps) {
            notes.push_back(note);
        }
    }

    for (const TimelineObject& entry : branch_section.timeline) {
        if (is_judge_timeline_entry(entry)) {
            judge_timeline.push_back(entry);
        }
    }
    std::ranges::stable_sort(judge_timeline, {}, &TimelineObject::start_time);

    auto by_hit = [](const Note& a, const Note& b) { return a.hit_ms < b.hit_ms; };
    for (const auto& note : notes) {
        if (note.type == NoteType::DON || note.type == NoteType::DON_L) {
            don_notes.insert(std::ranges::lower_bound(don_notes, note, by_hit), note);
        } else if (note.type == NoteType::KAT || note.type == NoteType::KAT_L) {
            kat_notes.insert(std::ranges::lower_bound(kat_notes, note, by_hit), note);
        } else if (note.type != NoteType::BARLINE) {
            other_notes.insert(std::ranges::lower_bound(other_notes, note, by_hit), note);
        }
    }
}

void JudgeCore::evaluate_branch(double current_ms) {
    float e_req = std::get<0>(curr_branch_reqs);
    float m_req = std::get<1>(curr_branch_reqs);
    double branch_end_ms = std::get<2>(curr_branch_reqs);
    if (current_ms < branch_end_ms) {
        return;
    }
    is_branch = false;
    if (branch_condition == "p") {
        branch_p_count =
            branch_note_count != 0
                ? std::max(
                      std::min(static_cast<int>(static_cast<double>(branch_p_count) / branch_note_count * 100), 100), 0)
                : 0;
    } else if (branch_condition == "r") {
        branch_r_count = std::max(curr_drumroll_count, branch_r_count);
    }
    float count = branch_condition == "p" ? branch_p_count : branch_r_count;
    BranchDifficulty chosen;
    if (branch_checkpoint_index < branch_history.size()) {
        chosen = branch_history[branch_checkpoint_index];
    } else {
        chosen = (count >= e_req && count < m_req && e_req >= 0) ? BranchDifficulty::EXPERT
                 : (count >= m_req)                              ? BranchDifficulty::MASTER
                                                                 : BranchDifficulty::NORMAL;
        branch_history.push_back(chosen);
    }
    branch_checkpoint_index++;
    spdlog::debug("Branch set to {} based on conditions {}, {}, {}", branch_diff_to_string(chosen), count, e_req,
                  m_req);

    std::deque<NoteList>& taken = chosen == BranchDifficulty::EXPERT   ? branch_e
                                  : chosen == BranchDifficulty::MASTER ? branch_m
                                                                       : branch_n;
    if (!taken.empty()) {
        NoteList section = std::move(taken.front());
        taken.pop_front();
        merge_branch_section(section, current_ms);
        std::deque<Note> merged;
        for (const Note& note : section.notes) {
            if (note.hit_ms >= resume_filter_ms - 1.0) {
                merged.push_back(note);
            }
        }
        on_branch_merged(chosen, merged, section);
    }
    for (auto* branch : {&branch_m, &branch_e, &branch_n}) {
        if (branch != &taken && !branch->empty()) {
            branch->pop_front();
        }
    }
    branch_p_count = 0;
    branch_r_count = 0;
    branch_note_count = 0;
}

void JudgeCore::play_note_manager(double current_ms) {
    const double miss_window = (difficulty <= 1) ? Timing::BAD_EASY : Timing::BAD;
    for (auto* lane : {&don_notes, &kat_notes}) {
        while (!lane->empty() && lane->front().hit_ms + miss_window < current_ms) {
            combo = 0;
            bad_count++;
            note_judgments[lane->front().index] = Judgments::BAD;
            if (judge_gauge.has_value()) {
                judge_gauge->add_bad();
            }
            Note missed = lane->front();
            lane->pop_front();
            branch_note_count++;
            on_judgment(missed, Judgments::BAD, std::nullopt);
        }
    }

    if (other_notes.empty()) {
        return;
    }

    Note& note = other_notes.front();
    if (note.hit_ms <= current_ms) {
        if (note.type == NoteType::ROLL_HEAD || note.type == NoteType::ROLL_HEAD_L) {
            is_drumroll = true;
        } else if (note.type == NoteType::BALLOON_HEAD || note.type == NoteType::KUSUDAMA) {
            if (!is_balloon) {
                on_balloon_start();
            }
            is_balloon = true;
        } else if (note.type == NoteType::TAIL) {
            other_notes.pop_front();
            is_drumroll = false;
            is_balloon = false;
            curr_drumroll_count = 0;
            curr_balloon_count = 0;
            return;
        }
        if (other_notes.size() < 2) {
            return;
        }
        Note& tail = other_notes[1];
        if (tail.hit_ms <= current_ms) {
            const bool balloon_missed = note.type == NoteType::BALLOON_HEAD; // a burst one is gone already
            other_notes.pop_front();
            other_notes.pop_front();
            is_drumroll = false;
            is_balloon = false;
            curr_drumroll_count = 0;
            curr_balloon_count = 0;
            if (balloon_missed) {
                on_balloon_missed();
            }
        }
    }
}

void JudgeCore::note_correct(const Note& note, double current_ms) {
    if (!don_notes.empty() && don_notes[0] == note) {
        don_notes.pop_front();
    } else if (!kat_notes.empty() && kat_notes[0] == note) {
        kat_notes.pop_front();
    } else if (!other_notes.empty() && other_notes[0] == note) {
        other_notes.pop_front();
    }

    if ((note.type == NoteType::BALLOON_HEAD || note.type == NoteType::KUSUDAMA) && !other_notes.empty()) {
        other_notes.pop_front();
    }

    if (note.type < NoteType::BALLOON_HEAD) {
        combo++;
        max_combo = std::max(max_combo, combo);
        if (combo % 100 == 0 && gen3) {
            score += 10000;
            on_score_popup(10000, false);
        }
    }
    on_note_correct(note, current_ms);
}

void JudgeCore::check_drumroll(double current_ms, DrumType drum_type) {
    curr_drumroll_count++;
    if (!other_notes.empty()) {
        roll_hits[other_notes.front().index] = curr_drumroll_count;
    }
    total_drumroll++;
    branch_r_count++;
    score += 100;
    on_score_popup(100, true);
    on_drumroll_hit(drum_type, current_ms);
}

void JudgeCore::check_balloon(double current_ms, DrumType drum_type, const Note& balloon) {
    if (drum_type != DrumType::DON || !balloon.count.has_value()) {
        return;
    }
    curr_balloon_count++;
    roll_hits[balloon.index] = curr_balloon_count;
    total_drumroll++;
    score += 100;
    on_score_popup(100, false);
    const bool popped = curr_balloon_count == balloon.count.value();
    on_balloon_hit(balloon, current_ms, popped);
    if (popped) {
        is_balloon = false;
        note_correct(balloon, current_ms);
        curr_balloon_count = 0;
    }
}

void JudgeCore::check_kusudama(double current_ms, DrumType drum_type, const Note& balloon) {
    if (drum_type != DrumType::DON || !balloon.count.has_value()) {
        return;
    }

    JudgeCore* owner = kusudama_owner();
    const long long key = std::llround(balloon.hit_ms);
    if (owner->kusudama_key != key) { // a new kusudama starts its count from zero
        owner->kusudama_key = key;
        owner->kusudama_shared_hits = 0;
    }
    total_drumroll++;
    score += 100;
    on_score_popup(100, false);
    owner->kusudama_shared_hits++;
    const bool popped = owner->kusudama_shared_hits == balloon.count.value();
    on_kusudama_hit(owner, balloon, current_ms, popped);

    if (popped) {
        is_balloon = false;
        note_correct(balloon, current_ms);

        JudgeCore* partner = judge_partner();
        if (partner && partner != this && partner->is_balloon && !partner->other_notes.empty() &&
            partner->other_notes.front().type == NoteType::KUSUDAMA) {
            Note partner_note = partner->other_notes.front();
            partner->is_balloon = false;
            partner->note_correct(partner_note, current_ms);
        }

        owner->kusudama_shared_hits = 0;
    }
}

void JudgeCore::check_note(double ms_from_start, DrumType drum_type, double current_ms) {
    if (don_notes.empty() && kat_notes.empty() && other_notes.empty()) {
        return;
    }

    auto record_hit_offset = [this, ms_from_start](const Note& note) {
        if (modifiers.auto_play) {
            return;
        }
        hit_offset_sum_ms += ms_from_start - note.hit_ms;
        hit_offset_count++;
    };

    float good_window_ms;
    float ok_window_ms;
    float bad_window_ms;
    if (difficulty <= 1) {
        good_window_ms = Timing::GOOD_EASY;
        ok_window_ms = Timing::OK_EASY;
        bad_window_ms = Timing::BAD_EASY;
    } else {
        good_window_ms = Timing::GOOD;
        ok_window_ms = Timing::OK;
        bad_window_ms = Timing::BAD;
    }
    if (gen3) {
        base_score = score_init;
        if (9 < combo && combo < 30) {
            base_score = std::floor(score_init + 1 * score_diff);
        } else if (29 < combo && combo < 50) {
            base_score = std::floor(score_init + 2 * score_diff);
        } else if (49 < combo && combo < 100) {
            base_score = std::floor(score_init + 4 * score_diff);
        } else if (99 < combo) {
            base_score = std::floor(score_init + 8 * score_diff);
        }
    }

    if (is_drumroll && !other_notes.empty()) {
        check_drumroll(current_ms, drum_type);
        return;
    }

    if (is_balloon && !other_notes.empty()) {
        Note balloon = other_notes.front();
        if (balloon.type == NoteType::BALLOON_HEAD) {
            check_balloon(current_ms, drum_type, balloon);
        }
        if (balloon.type == NoteType::KUSUDAMA) {
            check_kusudama(current_ms, drum_type, balloon);
        }
        return;
    }

    auto& lane = (drum_type == DrumType::DON) ? don_notes : kat_notes;
    if (lane.empty()) {
        return;
    }
    Note curr_note = lane.front();
    if (!modifiers.auto_play && lane.size() > 1 && ms_from_start > curr_note.hit_ms + ok_window_ms) {
        const Note& next = lane[1];
        auto blocked_by = [&](const std::deque<Note>& notes) {
            auto it = std::ranges::find_if(notes, [&](const Note& n) { return n.index > curr_note.index; });
            return it != notes.end() && it->index < next.index;
        };
        const auto& other_lane = (drum_type == DrumType::DON) ? kat_notes : don_notes;
        if (!blocked_by(other_lane) && !blocked_by(other_notes) && ms_from_start > next.hit_ms - ok_window_ms) {
            // hitting toward the next note gives up on this one
            combo = 0;
            bad_count++;
            note_judgments[curr_note.index] = Judgments::BAD;
            if (judge_gauge.has_value()) {
                judge_gauge->add_bad();
            }
            branch_note_count++;
            lane.pop_front();
            on_judgment(curr_note, Judgments::BAD, std::nullopt);
            curr_note = lane.front();
        }
    }

    if (ms_from_start > (curr_note.hit_ms + bad_window_ms)) {
        return;
    }

    if ((curr_note.hit_ms - good_window_ms <= ms_from_start) && (ms_from_start <= curr_note.hit_ms + good_window_ms)) {
        record_hit_offset(curr_note);
        note_judgments[curr_note.index] = Judgments::GOOD;
        good_count++;
        score += base_score;
        on_score_popup(base_score, true);
        on_judgment(curr_note, Judgments::GOOD, drum_type);
        note_correct(curr_note, current_ms);
        if (judge_gauge.has_value()) {
            judge_gauge->add_good();
        }
        branch_p_count++;
        branch_note_count++;
    } else if ((curr_note.hit_ms - ok_window_ms) <= ms_from_start &&
               ms_from_start <= (curr_note.hit_ms + ok_window_ms)) {
        record_hit_offset(curr_note);
        note_judgments[curr_note.index] = Judgments::OK;
        ok_count++;
        const int ok_score = 10 * std::floor(base_score / 2 / 10);
        score += ok_score;
        on_score_popup(ok_score, true);
        on_judgment(curr_note, Judgments::OK, drum_type);
        note_correct(curr_note, current_ms);
        if (judge_gauge.has_value()) {
            judge_gauge->add_ok();
        }
        branch_p_count += 0.5;
        branch_note_count++;
    } else if ((curr_note.hit_ms - bad_window_ms) <= ms_from_start &&
               ms_from_start <= (curr_note.hit_ms + bad_window_ms)) {
        bad_count++;
        combo = 0;
        branch_note_count++;
        record_hit_offset(curr_note);
        lane.pop_front();
        note_judgments[curr_note.index] = Judgments::BAD;
        if (judge_gauge.has_value()) {
            judge_gauge->add_bad();
        }
        on_judgment(curr_note, Judgments::BAD, drum_type);
    }
}
