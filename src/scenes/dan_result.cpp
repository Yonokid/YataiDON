#include "dan_result.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "../libs/file/filesystem.h"
#include "../libs/file/scores.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"

// the shoudan celebration and the congrats popup are skin art: a skin without the folder skips them
static bool has_dan_result_art(const char* subset) {
    return fs::exists(resolve_skin_path(fs::path("Graphics") / "dan_result" / subset));
}

namespace {
    constexpr double FRAME_MS = 1000.0 / 60.0;
    constexpr double LUA_FRAME_MS = 1000.0 / 120.0;
    constexpr double WAIT_TIME_MS = 0.5 * 1000.0;
    constexpr double WAIT_END_TIME_MS = 30.0 * 1000.0;
    constexpr double WAIT_DETAIL_MS = 0.2 * 1000.0;
    constexpr double SONG_LAND_MS[3] = {40 * FRAME_MS, 65 * FRAME_MS, 90 * FRAME_MS};
    constexpr double SONG_SLIDE_MS = 22 * FRAME_MS;
    constexpr double TOTAL_SLIDE_MS = 94 * FRAME_MS;
    constexpr double ROW_SLIDE_MS = 29 * FRAME_MS;
    constexpr double STAMP_ANM_MS = 89 * FRAME_MS;
    constexpr double DIGIT_ROLL_MS = 500.0;
    constexpr double GAUGE_UNIT_MS = 3 * LUA_FRAME_MS;
    constexpr double GAUGE_NUMIN_MS = 10 * FRAME_MS + 500.0;
    constexpr double ROW_UNIT_MS = 2 * LUA_FRAME_MS;
    constexpr double ROW_NUMIN_MS = 15 * FRAME_MS;
    constexpr double ROW_NUMWAIT_MS = 60 * LUA_FRAME_MS; // 500 ms
    constexpr double STAFFROLL_AT_MS = (249 + 240 + 30 + 120) * FRAME_MS;
    constexpr double STAFFROLL_END_MS = STAFFROLL_AT_MS + (420 + 4 * 270) * FRAME_MS;
    constexpr double CONGRATS_END_MS = 899 * FRAME_MS + 180 * LUA_FRAME_MS;

    int digit_count(int v) {
        int n = 1;
        while (v >= 10) {
            v /= 10;
            n++;
        }
        return n;
    }
} // namespace

void DanResultScreen::on_screen_start() {
    Screen::on_screen_start();
    audio.play_sound("bgm", VolumePreset::MUSIC);
    audio.play_sound("announce", VolumePreset::VOICE);
    audio.play_sound("partial_intro", VolumePreset::SOUND);

    fade_out = dynamic_cast<FadeAnimation*>(tex.get_animation(0));
    page2_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(1));
    if (!fade_out || !page2_fade) {
        throw std::runtime_error("DanResultScreen: animation 0/1 is not a FadeAnimation");
    }
    is_page2 = false;
    page_start_ms = get_current_ms();
    page1_start_ms = page_start_ms;

    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    background.emplace(PlayerNum::DAN, tex.screen_width);

    {
        auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num));
        chara = make_chara_from_player_data(pd ? &*pd : nullptr);
        if (pd) {
            chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
            chara->apply_face(pd->chara_face_index);
        } else {
            chara->set_don_colors(chara_default_color_1(global_data.player_num),
                                  chara_default_color_2(global_data.player_num), {249, 240, 225, 255});
        }
        chara->set_anim(AnimIndex::DON_NORMAL);
        nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", global_data.player_num, pd ? pd->dan : -1,
                              pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    }
    gauge_exam = -1;
    const DanResultData& rd = sd.dan_result_data;
    for (int i = 0; i < std::ssize(rd.exams) && i < std::ssize(rd.exam_data); i++) {
        if (rd.exams[i].type == "gauge") {
            gauge_exam = i;
            gauge_value = std::max(0, std::min(100, rd.exam_data[i].counter_value));
            gauge_border = std::max(0, std::min(100, rd.exams[i].red));
            break;
        }
    }

    page2_skipped = false;
    se_total_intro = se_countup = se_gauge_max = se_stamp = se_voice = false;
    se_advance = se_shogo = false;
    page1_plates_played = 0;
    celebrating = false;
    shodan = false;
    prev_best = 0;
    congrats_due = congrats_showing = false;
    congrats_start_ms = 0;
    se_congrats = se_staffroll = false;
    has_staffroll = has_dan_result_art("staffroll");
    prev_best_score = 0;
    best_score_show = false;
    prev_arrival = 0;
    nameplate_last_dan = -2;

    apply_reward();
    build_page2_timeline();

    draw_seq.emplace(sd.dan_result_data, prev_arrival, prev_best_score, best_score_show, gauge_exam, gauge_value,
                     gauge_border);
}

void DanResultScreen::apply_reward() {
    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const DanResultData& rd = sd.dan_result_data;
    if (rd.odai_result < 0) {
        return; // legacy record — no verdict
    }
    if (rd.skipped) {
        return; // skipped out: the cabinet records nothing
    }

    const std::string pid = get_player_id(global_data.player_num);
    const std::string course_id = dan_course_id(sd.selected_dan, rd.exams);
    auto prev = scores_manager.get_dan_record(pid, course_id);
    prev_best = prev ? prev->rank : 0;
    prev_best_score = prev ? prev->score : 0;
    prev_arrival = prev ? prev->arrival : 0;

    const int new_rank = rd.odai_result + 1; // 1 = played/failed, 2..7 = passes
    int arrival = 0;
    for (const auto& s : rd.songs) {
        if (!s.unreached) {
            arrival++;
        }
    }

    shodan = rd.odai_result > 0 && new_rank > prev_best && !rd.is_gaiden;

    best_score_show = rd.odai_result > 0 && prev_best > 0 && prev_best <= new_rank && rd.score > prev_best_score;

    congrats_due =
        shodan && rd.dan_index >= 0 && rd.dan_index_max >= 0 && rd.dan_index == rd.dan_index_max && prev_best <= 1;

    if (shodan && rd.dan_index >= 0) {
        if (auto pd = scores_manager.get_player_data(pid)) {
            if (pd->dan <= rd.dan_index) {
                pd->dan = rd.dan_index;
                pd->gold = rd.odai_result >= 4;
                pd->rainbow = rd.odai_result == 6;
                scores_manager.save_player_data(*pd);
                spdlog::info("Dan rank-up: '{}' -> nameplate dan={} gold={} rainbow={} (rank {} > prev {})",
                             rd.dan_title, pd->dan, pd->gold, pd->rainbow, new_rank, prev_best);
            }
        } else {
            spdlog::warn("Dan rank-up: '{}' could not update player profile (player data unavailable); dan record will "
                         "still be saved",
                         rd.dan_title);
        }
    }

    scores_manager.save_dan_score(pid, course_id, rd, arrival);

    std::string access_code;
    if (pid == scores_manager.player_1) {
        access_code = global_data.config->network.access_code_1;
    } else if (pid == scores_manager.player_2) {
        access_code = global_data.config->network.access_code_2;
    }
    if (!access_code.empty() && !is_guest_id(pid)) {
        network.submit_dan(access_code, course_id, rd, sd.selected_dan, arrival);
    }
}

void DanResultScreen::build_page2_timeline() {
    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const DanResultData& rd = sd.dan_result_data;

    totals_start = TOTAL_SLIDE_MS;

    int total_good = 0, total_ok = 0, total_bad = 0, total_dr = 0;
    for (const auto& s : rd.songs) {
        total_good += s.good;
        total_ok += s.ok;
        total_bad += s.bad;
        total_dr += s.drumroll;
    }
    int max_digits = digit_count(rd.score);
    for (int v :
         {total_good, total_ok, total_bad, total_dr, rd.max_combo, total_good + total_ok + total_bad + total_dr}) {
        max_digits = std::max(max_digits, digit_count(v));
    }
    double totals_dur = max_digits * DIGIT_ROLL_MS;
    if (gauge_exam >= 0) {
        totals_dur = std::max(totals_dur, gauge_value * GAUGE_UNIT_MS + GAUGE_NUMIN_MS);
    }
    totals_end = totals_start + totals_dur;

    const size_t row_count = std::min(rd.exams.size(), rd.exam_data.size());
    rows.assign(row_count, DanResultRowSchedule{});
    se_row_fill.assign(row_count, false);
    se_row_judge.assign(row_count, false);
    double t = totals_end + WAIT_DETAIL_MS;
    for (int i = 0; i < static_cast<int>(row_count); i++) {
        DanResultRowSchedule& r = rows[i];
        if (i == gauge_exam) {
            r.land = 0.0;
            r.fill0 = totals_start;
            r.filld = gauge_value * GAUGE_UNIT_MS;
            r.numin = totals_start + r.filld + GAUGE_NUMIN_MS - 500.0;
            continue;
        }
        const DanResultExam& re = rd.exam_data[i];
        float p = re.progress;

        float travel = (rd.exams[i].range == "less") ? (1.0f - p) : p;
        if (!rd.exams[i].gothrough) {
            travel = 0.0f;
            for (float sp : re.song_progress) {
                travel = std::max(travel, (rd.exams[i].range == "less") ? (1.0f - sp) : sp);
            }
        }
        r.land = t;
        r.fill0 = t + ROW_SLIDE_MS;
        r.filld = std::max(0.0, std::lround(travel * 100.0f) * ROW_UNIT_MS);
        r.numin = r.fill0 + r.filld + ROW_NUMIN_MS;
        t = r.numin + ROW_NUMWAIT_MS;
    }
    if (rows.empty()) {
        t = totals_end + WAIT_DETAIL_MS;
    }

    stamp_at = t + WAIT_TIME_MS;
    voice_at = stamp_at + STAMP_ANM_MS;
}

Screens DanResultScreen::on_screen_end(Screens next_screen) {
    reset_session();
    return Screen::on_screen_end(next_screen);
}

void DanResultScreen::handle_input(double current_ms) {
    const double on_page = current_ms - page_start_ms;
    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const int song_count = static_cast<int>(sd.dan_result_data.songs.size());
    const double page1_end = song_count > 0 ? (40 + (song_count - 1) * 25) * FRAME_MS + SONG_SLIDE_MS : 0.0;

    if (on_page < WAIT_TIME_MS) {
        return;
    }

    const bool don = is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num);

    const bool timed_out = on_page >= WAIT_END_TIME_MS;
    if (!don && !timed_out) {
        return;
    }

    if (congrats_showing) {
        return;
    }

    if (don) {
        audio.play_sound("don", VolumePreset::SOUND);
    }

    if (celebrating) {
        const double on_cel = current_ms - celebrate_start_ms;
        constexpr double CEL_EXIT_GATE_MS = 3500.0;
        if (on_cel >= CEL_EXIT_GATE_MS || timed_out) {
            if (congrats_due && has_dan_result_art("congrats")) {

                congrats_showing = true;
                congrats_start_ms = current_ms;
                celebrating = false;
            } else if (!fade_out->is_started) {
                fade_out->start();
            }
        }
        return;
    }

    if (is_page2) {

        if (!page2_skipped && on_page < stamp_at && !timed_out) {
            page2_skipped = true;
            totals_start = std::min(totals_start, on_page);
            totals_end = on_page;
            for (auto& r : rows) {
                r.land = std::min(r.land, on_page);
                r.fill0 = std::min(r.fill0, on_page);
                r.filld = 0.0;
                r.numin = on_page;
            }
            stamp_at = on_page + WAIT_TIME_MS;
            voice_at = stamp_at + STAMP_ANM_MS;
            return;
        }

        if (on_page < stamp_at + WAIT_TIME_MS && !timed_out) {
            return;
        }

        if (shodan && !celebrating) {
            if (has_dan_result_art("shoudan")) {
                celebrating = true;
                celebrate_start_ms = current_ms;
                return;
            }
            if (congrats_due && has_dan_result_art("congrats")) {
                congrats_showing = true;
                congrats_start_ms = current_ms;
                return;
            }
        }
        if (!fade_out->is_started) {
            fade_out->start();
        }
    } else if (don && current_ms - page1_start_ms < page1_end) {
        page1_start_ms = current_ms - WAIT_END_TIME_MS;
        page_start_ms = current_ms;
        page1_plates_played = song_count;
    } else {
        page2_fade->start();
        is_page2 = true;
        page_start_ms = current_ms;
    }
}

void DanResultScreen::update_sounds(double now) {
    const double on_page = now - page_start_ms;
    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const DanResultData& rd = sd.dan_result_data;

    if (congrats_showing) {
        if (!se_congrats) {
            se_congrats = true;
            audio.play_sound("voice_glad", VolumePreset::VOICE);
            audio.play_sound("popup_superlative", VolumePreset::SOUND);
        }
        if (!se_staffroll && now - congrats_start_ms >= STAFFROLL_AT_MS && has_staffroll) {
            se_staffroll = true;
            audio.stop_sound("bgm");
            audio.play_sound("staffroll", VolumePreset::MUSIC);
        }
        return;
    }

    if (celebrating) {
        const double on_cel = now - celebrate_start_ms;
        if (!se_advance) {
            se_advance = true;
            audio.play_sound("advance_intro", VolumePreset::SOUND);
            audio.play_sound("voice_advance", VolumePreset::VOICE);
        }
        if (!se_shogo && on_cel >= 3000.0) {
            se_shogo = true;
            audio.play_sound("advance_shogo", VolumePreset::SOUND);
        }
        return;
    }

    if (!is_page2) {
        while (page1_plates_played < std::ssize(rd.songs) && page1_plates_played < 3 &&
               on_page >= SONG_LAND_MS[page1_plates_played]) {
            audio.play_sound("partial_plate", VolumePreset::SOUND);
            page1_plates_played++;
        }
        return;
    }

    if (!se_total_intro) {
        se_total_intro = true;
        audio.play_sound("total_intro", VolumePreset::SOUND);
    }
    if (!se_countup && on_page >= totals_start && !page2_skipped) {
        se_countup = true;
        audio.play_sound("count_up_loop", VolumePreset::SOUND);
    }
    if (!se_gauge_max && gauge_exam >= 0 && gauge_value >= 100 &&
        on_page >= totals_start + gauge_value * GAUGE_UNIT_MS) {
        se_gauge_max = true;
        audio.play_sound("achieve_tamashii", VolumePreset::SOUND);
    }
    for (int i = 0; i < std::ssize(rows); i++) {
        if (i == gauge_exam) {
            continue;
        }
        if (!se_row_fill[i] && on_page >= rows[i].fill0 && rows[i].filld > 0 && !page2_skipped) {
            se_row_fill[i] = true;
            const bool less = i < std::ssize(rd.exams) && rd.exams[i].range == "less";
            audio.play_sound(less ? "gauge_down_loop" : "gauge_up_loop", VolumePreset::SOUND);
        }
        if (!se_row_judge[i] && on_page >= rows[i].numin && !page2_skipped) {
            se_row_judge[i] = true;
            audio.play_sound("gauge_judgement", VolumePreset::SOUND);
        }
    }
    if (!se_stamp && on_page >= stamp_at) {
        se_stamp = true;
        const int r = rd.odai_result;
        if (r == 0) {
            audio.play_sound("stamp_notclear", VolumePreset::SOUND);
        } else if (r < 0) {
            bool any_failed = std::ranges::any_of(rd.exam_data, [](const DanResultExam& e) { return e.failed; });
            audio.play_sound(any_failed ? "stamp_notclear" : "stamp_clear_normal", VolumePreset::SOUND);
        } else if (r <= 3) {
            audio.play_sound("stamp_clear_normal", VolumePreset::SOUND);
        } else {
            audio.play_sound("stamp_clear_upper", VolumePreset::SOUND);
        }
    }
    if (!se_voice && on_page >= voice_at) {
        se_voice = true;
        const int r = rd.odai_result;
        if (r == 0) {
            audio.play_sound("voice_notclear", VolumePreset::VOICE);
        } else if (r > 0) {
            audio.play_sound("atmos_clear", VolumePreset::SOUND);
            audio.play_sound(r <= 3 ? "voice_clear_normal" : "voice_clear_upper", VolumePreset::VOICE);
        }
    }
}

std::optional<Screens> DanResultScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();
    allnet_indicator.update(current_ms);

    handle_input(current_ms);
    update_sounds(current_ms);
    page2_fade->update(current_ms);
    fade_out->update(current_ms);
    nameplate.update(current_ms);
    if (chara) {
        chara->update(current_ms);
    }

    if (celebrating && current_ms - celebrate_start_ms >= 3000.0) {
        if (auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num))) {
            if (nameplate_last_dan != pd->dan) {
                nameplate = Nameplate(pd->username, pd->title, global_data.player_num, pd->dan, pd->gold, pd->rainbow,
                                      pd->title_bg);
                nameplate_last_dan = pd->dan;
            }
        }
    }

    const double congrats_end_ms = has_staffroll ? STAFFROLL_END_MS : CONGRATS_END_MS;
    if (congrats_showing && current_ms - congrats_start_ms >= congrats_end_ms && !fade_out->is_started) {
        fade_out->start();
    }

    if (fade_out->is_finished) {
        return on_screen_end(Screens::DAN_SELECT);
    }

    return std::nullopt;
}

void DanResultScreen::draw() {
    double now = get_current_ms();
    if (background.has_value()) {
        background->draw();
    }
    if (!draw_seq.has_value()) {
        return;
    }

    DanResultDraw::FrameState s;
    s.now = now;
    s.fade_out_attr = fade_out->attribute;
    s.page2_fade_attr = page2_fade->attribute;
    s.page_start_ms = page_start_ms;
    s.page1_start_ms = page1_start_ms;
    s.page2_skipped = page2_skipped;
    s.totals_start = totals_start;
    s.totals_end = totals_end;
    s.rows = rows;
    s.stamp_at = stamp_at;
    s.voice_at = voice_at;
    s.celebrating = celebrating;
    s.celebrate_start_ms = celebrate_start_ms;
    s.congrats_showing = congrats_showing;
    s.congrats_start_ms = congrats_start_ms;
    draw_seq->draw(s);

    auto [cx, cy] = tex.chara_xy("dan_result", "result");
    float cscale = 1.0f;
    draw_seq->chara_pos(cx, cy, cscale);
    if (chara) {
        chara->draw(cx, cy, cscale);
    }
    draw_seq->draw_overlay(s);

    auto [nx, ny] = tex.nameplate_xy("dan_result", "result");
    float nfade = 1.0f;
    draw_seq->nameplate_pos(nx, ny, nfade);
    nameplate.draw(nx, ny, nfade);

    coin_overlay.draw();
    allnet_indicator.draw();
}
