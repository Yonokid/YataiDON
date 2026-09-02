#include "dan_result.h"
#include <cmath>
#include "../libs/input.h"
#include "../libs/scores.h"

static float skin_outline(const SkinInfo& s) { return s.outline >= 0 ? s.outline : 5.0f; }

namespace {
constexpr double FRAME_MS     = 1000.0 / 60.0;
constexpr double LUA_FRAME_MS = 1000.0 / 120.0;
constexpr double WAIT_TIME_MS     = 0.5  * 1000.0;
constexpr double WAIT_END_TIME_MS = 30.0 * 1000.0;
constexpr double WAIT_DETAIL_MS   = 0.2  * 1000.0;
constexpr double SONG_LAND_MS[3] = { 40 * FRAME_MS, 65 * FRAME_MS, 90 * FRAME_MS };
constexpr double TOTAL_SLIDE_MS  = 94 * FRAME_MS;
constexpr double ROW_SLIDE_MS    = 29 * FRAME_MS;
constexpr double STAMP_ANM_MS    = 89 * FRAME_MS;
constexpr double DIGIT_ROLL_MS   = 500.0;
constexpr double GAUGE_UNIT_MS   = 3 * LUA_FRAME_MS;
constexpr double GAUGE_NUMIN_MS  = 10 * FRAME_MS + 500.0;
constexpr double ROW_UNIT_MS     = 2 * LUA_FRAME_MS;
constexpr double ROW_NUMIN_MS    = 15 * FRAME_MS;
constexpr double ROW_NUMWAIT_MS  = 60 * LUA_FRAME_MS;        // 500 ms

constexpr double SLIDE_MS       = 22 * FRAME_MS;
constexpr float  SLIDE_DIST     = 1600.0f;
constexpr double SLIDE_BREAK_AT = 5.0 / 22.0;
constexpr float  SLIDE_BREAK_X  = 35.0f;

float slide_offset(double t) {
    if (t <= 0)        return SLIDE_DIST;
    if (t >= SLIDE_MS) return 0.0f;
    double p = t / SLIDE_MS;
    if (p <= SLIDE_BREAK_AT)
        return (float)(SLIDE_DIST + (SLIDE_BREAK_X - SLIDE_DIST) * (p / SLIDE_BREAK_AT));
    return (float)(SLIDE_BREAK_X * (1.0 - (p - SLIDE_BREAK_AT) / (1.0 - SLIDE_BREAK_AT)));
}

int digit_count(int v) {
    int n = 1;
    while (v >= 10) { v /= 10; n++; }
    return n;
}

bool shoudan_glyphs(int di, int& ll, int& lr) {
    if (di == 0)               { ll = 10;      lr = 11; return true; }  // 初級
    if (di >= 1  && di <= 10)  { ll = 10 - di; lr = 11; return true; }  // 十級..一級
    if (di == 11)              { ll = 10;      lr = 12; return true; }  // 初段
    if (di >= 12 && di <= 20)  { ll = di - 11; lr = 12; return true; }  // 二段..十段
    if (di >= 21 && di <= 24)  { ll = di - 8;  lr = 17; return true; }  // 玄人..達人
    return false;
}
}  // namespace

void DanResultScreen::on_screen_start() {
    Screen::on_screen_start();
    audio.play_sound("bgm", VolumePreset::MUSIC);
    audio.play_sound("announce", VolumePreset::VOICE);
    audio.play_sound("partial_intro", VolumePreset::SOUND);

    fade_out   = (FadeAnimation*)tex.get_animation(0);
    page2_fade = (FadeAnimation*)tex.get_animation(1);
    is_page2   = false;
    page_start_ms = get_current_ms();
    page1_start_ms = page_start_ms;
    page1_armed.assign(
        global_data.session_data[(int)global_data.player_num].dan_result_data.songs.size(),
        false);

    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    background.emplace(PlayerNum::DAN, tex.screen_width);

    {
        auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num));
        chara = make_chara_from_player_data(pd ? &*pd : nullptr);
        if (pd) {
            chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
            chara->apply_face(pd->chara_face_index);
        } else {
            chara->set_don_colors(chara_default_color_1(get_player_id(global_data.player_num)),
                                  chara_default_color_2(get_player_id(global_data.player_num)),
                                  {249, 240, 225, 255});
        }
        chara->set_anim(AnimIndex::DON_NORMAL);
        nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "",
                              global_data.player_num,
                              pd ? pd->dan : -1, pd ? pd->gold : false,
                              pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    }
    //gauge = std::make_unique<Gauge>(Gauge::make_result(GaugeMode::DAN, global_data.player_num, sd.dan_result_data.gauge_length));

    int font_size = tex.skin_config[SC::DAN_TITLE].font_size;
    hori_name = std::make_unique<OutlinedText>(sd.dan_result_data.dan_title, font_size, ray::WHITE, ray::BLACK, false,
                                                skin_outline(tex.skin_config[SC::DAN_TITLE]));

    const SkinInfo& sn_cfg = tex.skin_config[SC::DAN_RESULT_SONG_NAME];
    int song_font_size = sn_cfg.font_size > 0 ? sn_cfg.font_size : font_size;
    gauge_exam = -1;
    const DanResultData& rd = sd.dan_result_data;
    for (int i = 0; i < (int)rd.exams.size() && i < (int)rd.exam_data.size(); i++) {
        if (rd.exams[i].type == "gauge") {
            gauge_exam   = i;
            gauge_value  = std::max(0, std::min(100, rd.exam_data[i].counter_value));
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
    se_congrats = false;
    prev_best_score = 0;
    best_score_show = false;

    apply_reward();
    build_page2_timeline();

    song_names.clear();
    for (int i = 0; i < (int)sd.dan_result_data.songs.size(); i++) {
        const DanResultSong& song = sd.dan_result_data.songs[i];
        std::string title = song.song_title;
        if (song.hidden && song.unreached && prev_arrival < i + 1)
            title = "？？？？？？";
        song_names.push_back(std::make_unique<OutlinedText>(title, song_font_size, ray::WHITE, ray::BLACK, false,
                                                              skin_outline(sn_cfg)));
    }
}

void DanResultScreen::apply_reward() {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;
    if (rd.odai_result < 0) return;               // legacy record — no verdict

    const int pid = get_player_id(global_data.player_num);
    auto prev = scores_manager.get_dan_record(pid, rd.dan_title);
    prev_best = prev ? prev->rank : 0;
    prev_best_score = prev ? prev->score : 0;
    prev_arrival    = prev ? prev->arrival : 0;

    const int new_rank = rd.odai_result + 1;      // 1 = played/failed, 2..7 = passes
    int arrival = 0;
    for (const auto& s : rd.songs)
        if (!s.unreached) arrival++;

    shodan = rd.odai_result > 0 && new_rank > prev_best && !rd.is_gaiden;

    best_score_show = rd.odai_result > 0 && prev_best > 0 &&
                      prev_best <= new_rank && rd.score > prev_best_score;

    congrats_due = shodan && rd.dan_index >= 0 &&
                   rd.dan_index_max >= 0 && rd.dan_index == rd.dan_index_max &&
                   prev_best <= 1;

    DanRecord rec;
    rec.dan_index = rd.dan_index;
    rec.rank      = std::max(prev_best, new_rank);
    rec.score     = std::max(prev_best_score, rd.score);
    rec.arrival   = std::max(prev ? prev->arrival : 0, arrival);
    scores_manager.save_dan_record(pid, rd.dan_title, rec);

    if (shodan && rd.dan_index >= 0) {
        if (auto pd = scores_manager.get_player_data(pid)) {
            if (pd->dan <= rd.dan_index) {
                pd->dan     = rd.dan_index;
                pd->gold    = rd.odai_result >= 4;
                pd->rainbow = rd.odai_result == 6;
                scores_manager.save_player_data(*pd);
                spdlog::info("Dan rank-up: '{}' -> nameplate dan={} gold={} rainbow={} (rank {} > prev {})",
                             rd.dan_title, pd->dan, pd->gold, pd->rainbow, new_rank, prev_best);
            }
        }
    }
}

void DanResultScreen::build_page2_timeline() {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;

    totals_start = TOTAL_SLIDE_MS;

    int total_good = 0, total_ok = 0, total_bad = 0, total_dr = 0;
    for (const auto& s : rd.songs) {
        total_good += s.good; total_ok += s.ok; total_bad += s.bad; total_dr += s.drumroll;
    }
    int max_digits = digit_count(rd.score);
    for (int v : { total_good, total_ok, total_bad, total_dr, rd.max_combo,
                   total_good + total_ok + total_bad + total_dr })
        max_digits = std::max(max_digits, digit_count(v));
    double totals_dur = max_digits * DIGIT_ROLL_MS;
    if (gauge_exam >= 0)
        totals_dur = std::max(totals_dur, gauge_value * GAUGE_UNIT_MS + GAUGE_NUMIN_MS);
    totals_end = totals_start + totals_dur;


    rows.assign(rd.exams.size(), RowSchedule{});
    se_row_fill.assign(rd.exams.size(), false);
    se_row_judge.assign(rd.exams.size(), false);
    double t = totals_end + WAIT_DETAIL_MS;
    for (int i = 0; i < (int)rd.exams.size() && i < (int)rd.exam_data.size(); i++) {
        RowSchedule& r = rows[i];
        if (i == gauge_exam) {
            r.land  = 0.0;
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
            for (int j = 0; j < 3; j++) {
                float sp = re.song_progress[j];
                travel = std::max(travel, (rd.exams[i].range == "less") ? (1.0f - sp) : sp);
            }
        }
        r.land  = t;
        r.fill0 = t + ROW_SLIDE_MS;
        r.filld = std::max(0.0, (double)std::lround(travel * 100.0f) * ROW_UNIT_MS);
        r.numin = r.fill0 + r.filld + ROW_NUMIN_MS;
        t = r.numin + ROW_NUMWAIT_MS;
    }
    if (rows.empty()) t = totals_end + WAIT_DETAIL_MS;

    stamp_at = t + WAIT_TIME_MS;
    voice_at = stamp_at + STAMP_ANM_MS;
}

Screens DanResultScreen::on_screen_end(Screens next_screen) {
    reset_session();
    exam_captions.clear();
    return Screen::on_screen_end(next_screen);
}

void DanResultScreen::handle_input(double current_ms) {
    const double on_page = current_ms - page_start_ms;

    if (on_page < WAIT_TIME_MS) return;

    const bool don = is_l_don_pressed(global_data.player_num) ||
                     is_r_don_pressed(global_data.player_num);

    const bool timed_out = on_page >= WAIT_END_TIME_MS;
    if (!don && !timed_out) return;

    if (congrats_showing) return;

    if (don) audio.play_sound("don", VolumePreset::SOUND);

    if (celebrating) {
        const double on_cel = current_ms - celebrate_start_ms;
        constexpr double CEL_EXIT_GATE_MS = 3500.0;
        if (on_cel >= CEL_EXIT_GATE_MS || timed_out) {
            if (congrats_due) {

                congrats_showing  = true;
                congrats_start_ms = current_ms;
                celebrating       = false;
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
            totals_end   = on_page;
            for (auto& r : rows) {
                r.land  = std::min(r.land,  on_page);
                r.fill0 = std::min(r.fill0, on_page);
                r.filld = 0.0;
                r.numin = on_page;
            }
            stamp_at = on_page + WAIT_TIME_MS;
            voice_at = stamp_at + STAMP_ANM_MS;
            return;
        }

        if (on_page < stamp_at + WAIT_TIME_MS && !timed_out) return;

        if (shodan && !celebrating) {

            celebrating = true;
            celebrate_start_ms = current_ms;
            return;
        }
        if (!fade_out->is_started) fade_out->start();
    } else {
        page2_fade->start();
        is_page2 = true;
        page_start_ms = current_ms;
    }
}

void DanResultScreen::update_sounds(double now) {
    const double on_page = now - page_start_ms;
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;

    if (congrats_showing) {
        if (!se_congrats) {
            se_congrats = true;
            audio.play_sound("voice_glad", VolumePreset::VOICE);
            audio.play_sound("popup_superlative", VolumePreset::SOUND);
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
        while (page1_plates_played < (int)rd.songs.size() && page1_plates_played < 3 &&
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
    for (int i = 0; i < (int)rows.size(); i++) {
        if (i == gauge_exam) continue;
        if (!se_row_fill[i] && on_page >= rows[i].fill0 && rows[i].filld > 0 && !page2_skipped) {
            se_row_fill[i] = true;
            const bool less = i < (int)rd.exams.size() && rd.exams[i].range == "less";
            audio.play_sound(less ? "gauge_down_loop" : "gauge_up_loop", VolumePreset::SOUND);
        }
        if (!se_row_judge[i] && on_page >= rows[i].numin) {
            se_row_judge[i] = true;
            audio.play_sound("gauge_judgement", VolumePreset::SOUND);
        }
    }
    if (!se_stamp && on_page >= stamp_at) {
        se_stamp = true;
        const int r = rd.odai_result;
        if (r == 0)      audio.play_sound("stamp_notclear",     VolumePreset::SOUND);
        else if (r < 0) {
            bool any_failed = std::any_of(rd.exam_data.begin(), rd.exam_data.end(),
                                          [](const DanResultExam& e){ return e.failed; });
            audio.play_sound(any_failed ? "stamp_notclear" : "stamp_clear_normal", VolumePreset::SOUND);
        }
        else if (r <= 3) audio.play_sound("stamp_clear_normal", VolumePreset::SOUND);
        else             audio.play_sound("stamp_clear_upper",  VolumePreset::SOUND);
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
    if (chara) chara->update(current_ms);

    if (congrats_showing &&
        current_ms - congrats_start_ms >= 899 * FRAME_MS + 180 * LUA_FRAME_MS) {
        if (!fade_out->is_started) fade_out->start();
    }

    if (fade_out->is_finished)
        return on_screen_end(Screens::DAN_SELECT);

    return std::nullopt;
}

void DanResultScreen::draw_chara_and_plate() {
    const SkinInfo* c = tex.skin_entry("dan_result_chara");
    const SkinInfo* n = tex.skin_entry("dan_result_nameplate");
    const SkinInfo& cs = c ? *c : tex.skin_config[SC::RESULT_CHARA];
    const SkinInfo& ns = n ? *n : tex.skin_config[SC::RESULT_NAMEPLATE];
    if (chara) chara->draw(cs.x, cs.y);
    nameplate.draw(ns.x, ns.y);
}

void DanResultScreen::draw_digit_counter(const std::string& digits, float margin_x, TexID id,
                                          int index, float y, double fade, float scale,
                                          float x_off, double roll_t) {
    const int n = (int)digits.size();
    for (int j = 0; j < n; j++) {
        const int r = n - 1 - j;              // position from the right (0 = ones)
        int frame = digits[j] - '0';
        if (roll_t >= 0.0) {
            if (roll_t < r * DIGIT_ROLL_MS) continue;                  // "none"
            if (roll_t < (r + 1) * DIGIT_ROLL_MS)
                frame = (int)(roll_t / 83.0 + r) % 10;                 // rolling
        }
        float x = x_off - (float)(n - j) * margin_x;
        tex.draw_texture(id, {.frame=frame, .scale=scale, .x=x, .y=y, .fade=fade, .index=index});
    }
}

void DanResultScreen::draw_page1(double now) {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    float height = tex.skin_config[SC::DAN_RESULT_INFO_HEIGHT].y;
    float margin = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].x;
    const double on_page = now - page1_start_ms;

    for (int i = 0; i < (int)sd.dan_result_data.songs.size(); i++) {
        const DanResultSong& song = sd.dan_result_data.songs[i];
        float y = i * height;

        double land = (i < 3) ? SONG_LAND_MS[i]
                              : SONG_LAND_MS[2] + (i - 2) * 25 * FRAME_MS;
        if (on_page < land) continue;
        float sx = slide_offset(on_page - land);
        page1_armed[i] = true;

        const float rf = song.unreached ? 0.45f : 1.0f;

        tex.draw_texture(BACKGROUND::GENRE_BANNER, {.frame=song.genre_index, .x=sx, .y=y, .fade=rf});

        if (i < (int)song_names.size() && song_names[i]) {
            const SkinInfo& sn = tex.skin_config[SC::DAN_RESULT_SONG_NAME];
            float tw = song_names[i]->width;
            float draw_w = sn.width > 0 ? std::min(tw, sn.width) : tw;
            song_names[i]->draw({.x = sn.x + sx, .y = y + sn.y, .x2 = draw_w - tw});
        }

        tex.draw_texture(RESULT_INFO::SONG_NUM,   {.frame=i, .x=sx, .y=y, .fade=rf});
        tex.draw_texture(RESULT_INFO::DIFFICULTY, {.frame=song.selected_difficulty, .x=sx, .y=y, .fade=rf});
        tex.draw_texture(RESULT_INFO::DIFF_STAR,  {.x=sx, .y=y, .fade=rf});
        tex.draw_texture(RESULT_INFO::DIFF_X,     {.x=sx, .y=y, .fade=rf});

        std::string lv = std::to_string(song.diff_level);
        float dm = tex.skin_config[SC::DAN_RESULT_DIFF_NUM_MARGIN].x;
        const SkinInfo* dl = tex.skin_entry("dan_result_diff_num_lead");
        const float lead = (dl ? dl->x : 0.0f) * (float)(lv.size() - 1);
        for (int j = 0; j < (int)lv.size(); j++)
            tex.draw_texture(RESULT_INFO::DIFF_NUM,
                             {.frame=lv[j]-'0', .x=sx - lead + (float)(j*dm), .y=y});

        auto draw_stat = [&](TexID icon, int val, int idx) {
            tex.draw_texture(icon, {.x=sx, .y=y, .fade=rf});
            if (song.unreached) return;
            std::string sv = std::to_string(val);
            std::reverse(sv.begin(), sv.end());
            for (int j = 0; j < (int)sv.size(); j++)
                tex.draw_texture(RESULT_INFO::COUNTER, {.frame=sv[j]-'0', .x=sx-(float)(j*margin), .y=y, .index=idx});
        };
        draw_stat(RESULT_INFO::GOOD,     song.good,     0);
        draw_stat(RESULT_INFO::OK,       song.ok,       1);
        draw_stat(RESULT_INFO::BAD,      song.bad,      2);
        draw_stat(RESULT_INFO::DRUMROLL, song.drumroll, 3);
        if (song.unreached && tex.has_texture("result_info/unreached"))
            tex.draw_texture(tex.get_enum("result_info/unreached"), {.x=sx, .y=y});
    }
}

void DanResultScreen::draw_gauge_row(const Exam& exam, float y, double fade, double now, float scale) {
    const double on_page = now - page_start_ms;
    const int final_cells  = gauge_value  / 2;
    const int border_cells = gauge_border / 2;

    int cur = gauge_value;
    bool landed = true;
    if (!page2_skipped && on_page < totals_start + gauge_value * GAUGE_UNIT_MS) {
        cur = (int)std::max(0.0, (on_page - totals_start) / GAUGE_UNIT_MS);
        if (cur > gauge_value) cur = gauge_value;
        landed = false;
    }
    const int cells = cur / 2;
    const bool clear = cur >= gauge_border && gauge_border > 0;

    auto T = [&](const char* n) { return tex.get_enum(std::string("tamashii/") + n); };

    tex.draw_texture(T("base"), {.scale=scale, .y=y, .fade=fade});
    if (border_cells > 0)
        tex.draw_texture(T("norma"), {.scale=scale, .y=y, .x2=scale*21.0f*border_cells, .fade=fade});
    if (cells > 0)
        tex.draw_texture(T(clear ? "fill_clear" : "fill_1p"),
                         {.scale=scale, .y=y, .x2=scale*21.0f*cells, .fade=fade});
    tex.draw_texture(T("overlay"), {.scale=scale, .y=y, .fade=fade});

    const bool numin = page2_skipped ||
                       on_page >= totals_start + gauge_value * GAUGE_UNIT_MS + (GAUGE_NUMIN_MS - 500.0);
    if (numin) {
        const char* pal = gauge_value >= 100 ? "max" : clear ? "clear" : "noclear";
        std::string s = std::to_string(gauge_value);
        const int n = (int)s.size() + 1;                        // digits + %
        const float pitch = 34.0f * scale;
        TexID numtex = T((std::string("num_") + pal).c_str());
        TexID pcttex = T((std::string("pct_") + pal).c_str());
        for (int k = 0; k < n; k++) {
            float dx = (k - (n - 1) / 2.0f) * pitch;
            if (k < n - 1)
                tex.draw_texture(numtex, {.frame=s[k]-'0', .scale=scale, .x=dx, .y=y, .fade=fade});
            else
                tex.draw_texture(pcttex, {.scale=scale, .x=dx, .y=y, .fade=fade});
        }
    }

    const bool fever = gauge_value >= 100 && landed;
    if (fever && tex.has_texture("tamashii/flame")) {
        const double t0 = page2_skipped ? 0.0
                          : totals_start + gauge_value * GAUGE_UNIT_MS;
        const double f = 20.0 + std::max(0.0, on_page - t0) / FRAME_MS;
        const double flame_a = std::min(1.0, (f - 20.0) / 4.0);
        const int cel = f < 29.0 ? 0 : ((int)((f - 29.0) / 3.0)) % 8;
        tex.draw_texture(T("flame"), {.frame=cel, .scale=scale, .y=y,
                                      .fade=fade * flame_a});
        const bool tick = ((int)((f - 29.0) / 2.0)) % 2 == 0;
        tex.draw_texture(T(f >= 29.0 && tick ? "tamashii_dark" : "tamashii_lit"),
                         {.scale=scale, .y=y, .fade=fade});
        if (f >= 24.0 && (f < 29.0 || tick) && tex.has_texture("tamashii/fever_glow"))
            tex.draw_texture(T("fever_glow"), {.scale=scale, .y=y, .fade=fade * 0.6});
    } else {
        tex.draw_texture(T(clear && landed ? "tamashii_lit" : "tamashii_dark"),
                         {.scale=scale, .y=y, .fade=fade});
    }
    tex.draw_texture(T("marker"), {.scale=scale, .x=scale*21.0f*border_cells, .y=y, .fade=fade});
}

void DanResultScreen::draw_exam_info(double fade, double now, float scale) {
    const SessionData& sd  = global_data.session_data[(int)global_data.player_num];
    const double on_page = now - page_start_ms;
    const SkinInfo& dei = tex.skin_entry("dan_exam_info_result")
                        ? *tex.skin_entry("dan_exam_info_result")
                        : tex.skin_config[SC::DAN_EXAM_INFO];
    float offset_y = dei.y * scale;
    float margin   = dei.x * scale;

    const SkinInfo* sr = tex.skin_entry("dan_result_exam_sub_row");
    const float sub_pitch = (sr ? sr->y : 465.0f) * scale;
    const SkinInfo* sb = tex.skin_entry("dan_result_exam_sub_bar");
    const float sub_full   = sb ? sb->width : 324.0f;
    const float sub_margin = (sb ? sb->x : 27.0f) * scale;

    const bool has_border_digits = tex.has_texture("exam_info/exam_border_counter");
    const TexID border_id = has_border_digits ? EXAM_INFO::EXAM_BORDER_COUNTER
                                              : EXAM_INFO::VALUE_COUNTER;
    const float border_pitch = (has_border_digits ? 26.0f : margin) * scale;
    const float sub_border_scale = 20.0f / 36.0f;

    auto pens = [&](TexID id) {
        auto it = tex.textures.find((uint32_t)id);
        return (it == tex.textures.end()) ? 1 : (int)it->second->x.size();
    };
    const int sub_pen    = pens(EXAM_INFO::EXAM_LESS) > 1 ? 1 : 0;
    const int sub_bd_pen = pens(border_id)            > 1 ? 1 : 0;

    auto digits_left = [&](const std::string& s, float x0, float y0, float pitch,
                           TexID id, int index, float sc) {
        for (int k = 0; k < (int)s.size(); k++)
            tex.draw_texture(id, {.frame=s[k]-'0', .scale=sc, .x=x0 + k*pitch,
                                  .y=y0, .fade=fade, .index=index});
    };

    auto have = [&](TexID id) {
        return tex.textures.find((uint32_t)id) != tex.textures.end();
    };
    auto draw_exam_fill = [&](const std::string& state, float w, float rx, float ry,
                              float sc, double fd, double t, bool sub) {
        if (w <= 0 || state == "empty") return;
        const TexID rb   = sub ? EXAM_INFO::EXAM_SUB_RAINBOW : EXAM_INFO::EXAM_RAINBOW;
        const TexID d80  = sub ? EXAM_INFO::EXAM_SUB_DOWN80  : EXAM_INFO::EXAM_DOWN80;
        const TexID up50 = sub ? EXAM_INFO::EXAM_SUB_RED     : EXAM_INFO::EXAM_RED;
        const TexID up80 = sub ? EXAM_INFO::EXAM_SUB_GOLD    : EXAM_INFO::EXAM_GOLD;
        const TexID p100 = sub ? EXAM_INFO::EXAM_SUB_MAX     : EXAM_INFO::EXAM_MAX;
        if (state == "max" && have(rb)) {
            auto it = tex.textures.find((uint32_t)rb);
            const float th   = (float)it->second->height;
            const float perd = (float)it->second->width * 0.5f;   // two periods
            (void)t;
            const float ph   = perd - (float)std::fmod(get_frame_ms() / 1000.0
                                                       * (perd * 60.0 / 80.0),
                                                       (double)perd);
            const float sw = w / sc;                   // source width, tile units
            const float avail = 2.0f * perd - ph;
            if (sw <= avail) {
                tex.draw_texture(rb, {.scale=sc, .x=rx, .y=ry, .x2=w, .fade=fd,
                                      .src=ray::Rectangle{ph, 0.0f, sw, th}});
            } else {
                tex.draw_texture(rb, {.scale=sc, .x=rx, .y=ry, .x2=avail * sc,
                                      .fade=fd,
                                      .src=ray::Rectangle{ph, 0.0f, avail, th}});
                tex.draw_texture(rb, {.scale=sc, .x=rx + avail * sc, .y=ry,
                                      .x2=(sw - avail) * sc, .fade=fd,
                                      .src=ray::Rectangle{0.0f, 0.0f, sw - avail, th}});
            }
            return;
        }
        TexID id = p100;
        if (state == "up_50")        id = up50;
        else if (state == "up_80")   id = up80;
        else if (state == "down_80") id = have(d80) ? d80 : up80;
        tex.draw_texture(id, {.scale=sc, .x=rx, .y=ry, .x2=w, .fade=fd});
    };

    for (int i = 0; i < (int)sd.dan_result_data.exams.size(); i++) {
        const Exam& exam       = sd.dan_result_data.exams[i];
        const DanResultExam& rd = sd.dan_result_data.exam_data[i];
        float y = i * offset_y;

        const RowSchedule rs = i < (int)rows.size() ? rows[i] : RowSchedule{};
        if (on_page < rs.land) continue;

        double frac = 1.0;
        if (!page2_skipped && rs.filld > 0)
            frac = std::max(0.0, std::min(1.0, (on_page - rs.fill0) / rs.filld));
        const bool row_numin = page2_skipped || on_page >= rs.numin;

        tex.draw_texture(EXAM_INFO::EXAM_BG, {.scale=scale, .y=y, .fade=fade});

        const bool tamashii_row = (i == gauge_exam) && tex.has_texture("tamashii/base");

        if (tamashii_row) {
            draw_gauge_row(exam, y, fade, now, scale);
        } else if (exam.gothrough) {
            tex.draw_texture(EXAM_INFO::EXAM_BADGE, {.frame=0, .scale=scale, .y=y, .fade=fade});
            tex.draw_texture(EXAM_INFO::EXAM_OVERLAY_1, {.scale=scale, .y=y, .fade=fade});

            float p = rd.progress;
            if (exam.range == "less") p = 1.0f - (1.0f - p) * (float)frac;
            else                      p = p * (float)frac;
            float bar_w = dei.width * p * scale;
            std::string bs = rd.bar_state;
            if (frac < 1.0) {
                const int g = (int)std::floor(p * 100.0f);
                bs = g <= 0 ? "empty" : g <= 49 ? "up_50" : g <= 99 ? "up_80" : "up_100";
            }
            draw_exam_fill(bs, bar_w, 0.0f, y, scale, fade, now, false);
        } else {
            int reach = 0;
            for (const auto& s : sd.dan_result_data.songs)
                if (!s.unreached) reach++;
            if (reach <= 0) reach = std::max(1, rd.song_count + 1);
            reach = std::min(reach, 3);
            for (int j = 0; j < reach; j++) {
                float sx = j * sub_pitch;
                tex.draw_texture(EXAM_INFO::EXAM_BADGE, {.frame=j + 1, .scale=scale, .x=sx, .y=y, .fade=fade});
                tex.draw_texture(EXAM_INFO::EXAM_SUB_BG, {.scale=scale, .x=sx, .y=y, .fade=fade});
                float sp = rd.song_progress[j];
                if (exam.range == "less") sp = 1.0f - (1.0f - sp) * (float)frac;
                else                      sp = sp * (float)frac;
                std::string sbs = rd.song_state[j];
                if (frac < 1.0) {
                    const int g = (int)std::floor(sp * 100.0f);
                    sbs = g <= 0 ? "empty" : g <= 49 ? "up_50"
                        : g <= 99 ? "up_80" : "up_100";
                }
                draw_exam_fill(sbs, sub_full * sp, sx, y, scale, fade, now, true);
                tex.draw_texture(EXAM_INFO::EXAM_SUB_FRONT, {.scale=scale, .x=sx, .y=y, .fade=fade});
                const SkinInfo* sbt = tex.skin_entry("dan_result_exam_border_song");
                OutlinedText* scap = nullptr;
                float sbt_ol = 3.5f / tex.screen_scale;
                if (sbt) {
                    if (sbt->outline >= 0) sbt_ol = sbt->outline;
                    scap = exam_captions.get(
                        exam_threshold_text(tex, exam.type, exam.range, exam.red,
                                            global_data.config->general.language),
                        sbt->font_size > 0 ? sbt->font_size : 20, sbt_ol);
                }
                if (scap) {
                    const float pad = ExamCaptionCache::pad_for(sbt_ol, tex.screen_scale);
                    scap->draw({.x = sbt->x + sx - scap->width + pad,
                                .y = sbt->y - pad + y, .fade = fade});
                } else {
                    const std::string bs = std::to_string(exam.red);
                    const float bx = sx;
                    digits_left(bs, bx, y, border_pitch * sub_border_scale,
                                border_id, sub_bd_pen, scale * sub_border_scale);
                    const float after = bx + bs.size() * border_pitch * sub_border_scale
                                      + 8.0f * scale * sub_border_scale;
                    if (exam.range == "less")
                        tex.draw_texture(EXAM_INFO::EXAM_LESS, {.scale=scale*sub_border_scale, .x=after, .y=y, .fade=fade, .index=sub_pen});
                    else if (exam.range == "more")
                        tex.draw_texture(EXAM_INFO::EXAM_MORE, {.scale=scale*sub_border_scale, .x=after, .y=y, .fade=fade, .index=sub_pen});
                }
                if (row_numin) {
                    const bool smax = frac >= 1.0 && rd.song_state[j] == "max"
                                    && have(EXAM_INFO::EXAM_SUB_COUNTER_MAX);
                    digits_left(std::to_string(rd.song_value[j]), sx, y, sub_margin,
                                smax ? EXAM_INFO::EXAM_SUB_COUNTER_MAX
                                     : EXAM_INFO::EXAM_SUB_COUNTER, 0, scale);
                }
            }
        }

        static const std::unordered_map<std::string, TexID> icon_ids = {
            {"gauge",        EXAM_INFO::EXAM_GAUGE},
            {"combo",        EXAM_INFO::EXAM_COMBO},
            {"hit",          EXAM_INFO::EXAM_HIT},
            {"judgebad",     EXAM_INFO::EXAM_JUDGEBAD},
            {"judgegood",    EXAM_INFO::EXAM_JUDGEGOOD},
            {"judgeperfect", EXAM_INFO::EXAM_JUDGEPERFECT},
            {"score",        EXAM_INFO::EXAM_SCORE},
            {"renda",        EXAM_INFO::EXAM_ROLL},
        };
        auto icon_it = icon_ids.find(exam.type);
        if (icon_it != icon_ids.end())
            tex.draw_texture(icon_it->second, {.scale=scale, .y=y, .fade=fade});

        if (exam.gothrough || tamashii_row) {
        const std::string red_str = std::to_string(exam.red);

        const float csc  = tamashii_row ? (25.0f / 36.0f) : 1.0f;
        const float gap1 = 4.0f * scale * csc, gap2 = 10.0f * scale * csc;
        float cap_w = red_str.size() * border_pitch * csc;
        if (exam.type == "gauge") cap_w += gap1 + 31.0f * scale * csc;
        if (exam.range == "less" || exam.range == "more")
            cap_w += gap2 + 70.0f * scale * csc;
        float cap_dx = 0.0f, cap_dy = 0.0f;
        if (tamashii_row) {
            cap_dx = scale * (21.0f * (gauge_border / 2) + 52.0f) - cap_w * 0.5f;
            cap_dy = 52.0f * scale;
        }
        const SkinInfo* bt = tex.skin_entry(tamashii_row ? "dan_result_gauge_border_text"
                                                         : "dan_result_exam_border_text");
        OutlinedText* capt = nullptr;
        float bt_ol = 4.0f / tex.screen_scale;
        if (bt) {
            if (bt->outline >= 0) bt_ol = bt->outline;
            capt = exam_captions.get(
                exam_threshold_text(tex, exam.type, exam.range, exam.red,
                                    global_data.config->general.language),
                bt->font_size > 0 ? bt->font_size : (tamashii_row ? 25 : 36), bt_ol);
        }
        if (capt) {
            const float pad = ExamCaptionCache::pad_for(bt_ol, tex.screen_scale);
            const float cx = tamashii_row
                ? bt->x + scale * (21.0f * (gauge_border / 2) + 52.0f) - capt->width * 0.5f
                : bt->x - capt->width + pad;
            capt->draw({.x = cx, .y = bt->y - pad + y, .fade = fade});
        } else {
        digits_left(red_str, cap_dx, y + cap_dy, border_pitch * csc, border_id, 0, scale * csc);
        float after = cap_dx + red_str.size() * border_pitch * csc;
        if (exam.type == "gauge") {
            tex.draw_texture(EXAM_INFO::EXAM_PERCENT, {.scale=scale*csc, .x=after + gap1, .y=y + cap_dy, .fade=fade, .index=0});
            after += gap1 + 31.0f * scale * csc;
        }
        after += gap2;
        if (exam.range == "less")
            tex.draw_texture(EXAM_INFO::EXAM_LESS, {.scale=scale*csc, .x=after, .y=y + cap_dy, .fade=fade});
        else if (exam.range == "more")
            tex.draw_texture(EXAM_INFO::EXAM_MORE, {.scale=scale*csc, .x=after, .y=y + cap_dy, .fade=fade});
        }

        if (!tamashii_row) {
            tex.draw_texture(EXAM_INFO::EXAM_OVERLAY_2, {.scale=scale, .y=y, .fade=fade});
            if (row_numin) {
                const int vpen = pens(EXAM_INFO::VALUE_COUNTER) > 1 ? 1 : 0;
                const int ppen = pens(EXAM_INFO::EXAM_PERCENT)  > 1 ? 1 : 0;
                const std::string cur_str = std::to_string(rd.counter_value);
                const bool vmax = frac >= 1.0 && rd.bar_state == "max"
                                && have(EXAM_INFO::VALUE_COUNTER_MAX);
                digits_left(cur_str, 0.0f, y, margin,
                            vmax ? EXAM_INFO::VALUE_COUNTER_MAX
                                 : EXAM_INFO::VALUE_COUNTER, vpen, scale);
                if (exam.type == "gauge")
                    tex.draw_texture(EXAM_INFO::EXAM_PERCENT,
                                     {.scale=scale, .x=cur_str.size()*margin + 6.0f*scale,
                                      .y=y, .fade=fade, .index=ppen});
            }
        }
        }

    }
}

void DanResultScreen::draw_page2(double fade, double now) {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;
    const double on_page = now - page_start_ms;

    tex.draw_texture(BACKGROUND::RESULT_2_BG, {.fade=fade});
    for (int i = 0; i < 5; i++)
        tex.draw_texture(BACKGROUND::RESULT_2_DIVIDER, {.x=(float)(i*240), .fade=fade});
    tex.draw_texture(BACKGROUND::RESULT_2_PULLOUT, {.fade=fade});

    if (rd.dan_rank >= 0 && tex.options[SCO::DAN_RESULT_RANK_PLATE]) {
        tex.draw_texture(RESULT_INFO::RANK_PLATE, {.frame=rd.dan_rank});
    } else {
        tex.draw_texture(RESULT_INFO::DAN_EMBLEM, {.frame=rd.dan_color});
        if (hori_name) {
            SkinInfo hn = tex.skin_config[SC::DAN_RESULT_HORI_NAME];
            hori_name->draw({
                .x = hn.x - hori_name->width/2.0f,
                .y = hn.y,
                .x2= std::min(hori_name->width, hn.width) - hori_name->width
            });
        }
    }
    draw_chara_and_plate();

    float margin = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].x;

    int total_good     = 0, total_ok = 0, total_bad = 0, total_dr = 0;
    for (const auto& s : rd.songs) {
        total_good += s.good; total_ok += s.ok; total_bad += s.bad; total_dr += s.drumroll;
    }

    const bool totals_landed = page2_skipped || on_page >= totals_end;
    const double roll_t = totals_landed ? -1.0 : (on_page - totals_start);
    if (totals_landed || on_page >= totals_start) {
        auto draw_total = [&](TexID icon, int idx_icon, int total, int idx_counter) {
            tex.draw_texture(icon, {.fade=fade, .index=idx_icon});
            draw_digit_counter(std::to_string(total), margin, RESULT_INFO::COUNTER,
                               idx_counter, 0.0f, fade, 1.0f, margin, roll_t);
        };
        draw_total(RESULT_INFO::GOOD,     1, total_good, 4);
        draw_total(RESULT_INFO::OK,       1, total_ok,   5);
        draw_total(RESULT_INFO::BAD,      1, total_bad,  6);
        draw_total(RESULT_INFO::DRUMROLL, 1, total_dr,   7);

        tex.draw_texture(RESULT_INFO::MAX_COMBO, {.fade=fade});
        draw_digit_counter(std::to_string(rd.max_combo), margin, RESULT_INFO::COUNTER,
                           8, 0.0f, fade, 1.0f, margin, roll_t);

        int total_hits = total_good + total_ok + total_bad + total_dr;
        tex.draw_texture(RESULT_INFO::MAX_HITS, {.fade=fade});
        draw_digit_counter(std::to_string(total_hits), margin, RESULT_INFO::COUNTER,
                           9, 0.0f, fade, 1.0f, margin, roll_t);
    } else {
        tex.draw_texture(RESULT_INFO::GOOD,     {.fade=fade, .index=1});
        tex.draw_texture(RESULT_INFO::OK,       {.fade=fade, .index=1});
        tex.draw_texture(RESULT_INFO::BAD,      {.fade=fade, .index=1});
        tex.draw_texture(RESULT_INFO::DRUMROLL, {.fade=fade, .index=1});
        tex.draw_texture(RESULT_INFO::MAX_COMBO,{.fade=fade});
        tex.draw_texture(RESULT_INFO::MAX_HITS, {.fade=fade});
    }

    tex.draw_texture(RESULT_INFO::EXAM_HEADER, {.fade=fade});

    // Score box (rolls with the same clock, its own margin)
    tex.draw_texture(RESULT_INFO::SCORE_BOX, {.fade=fade});
    float sm = tex.skin_config[SC::DAN_SCORE_BOX_MARGIN].x;
    if (totals_landed || on_page >= totals_start)
        draw_digit_counter(std::to_string(rd.score), sm, RESULT_INFO::SCORE_COUNTER,
                           0, 0.0f, fade, 1.0f, sm, roll_t);

    if (best_score_show)
        draw_best_score(fade, on_page);

    draw_exam_info(fade, now);

    if (on_page < stamp_at) return;

    if (rd.odai_result >= 0) {
        const int r = rd.odai_result;
        if (r == 0) {
            tex.draw_texture(EXAM_INFO::FAIL, {.fade=fade});
        } else {
            const bool gold = r >= 4;
            const int  sub  = gold ? r - 3 : r;           // 1 plain, 2 FC, 3 全良
            const std::string base = gold ? "exam_info/gold_clear" : "exam_info/red_clear";
            const std::string variant = base + "_0" + std::to_string(sub);
            if (sub > 1 && tex.has_texture(variant))
                tex.draw_texture(tex.get_enum(variant), {.fade=fade});
            else
                tex.draw_texture(gold ? EXAM_INFO::GOLD_CLEAR : EXAM_INFO::RED_CLEAR, {.fade=fade});
        }
    } else {
        bool any_failed = std::any_of(rd.exam_data.begin(), rd.exam_data.end(),
                                       [](const DanResultExam& e){ return e.failed; });
        bool all_gold   = !any_failed && !rd.exams.empty() && !rd.exam_data.empty();
        if (all_gold) {
            for (int i = 0; i < (int)rd.exams.size() && i < (int)rd.exam_data.size(); i++) {
                if (rd.exam_data[i].progress < (float)rd.exams[i].gold / (float)(rd.exams[i].red > 0 ? rd.exams[i].red : 1)) {
                    all_gold = false; break;
                }
            }
        }
        if (any_failed)
            tex.draw_texture(EXAM_INFO::FAIL,       {.fade=fade});
        else if (all_gold)
            tex.draw_texture(EXAM_INFO::GOLD_CLEAR, {.fade=fade});
        else
            tex.draw_texture(EXAM_INFO::RED_CLEAR,  {.fade=fade});
    }
}

void DanResultScreen::draw_best_score(double fade, double on_page) {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;
    if (!tex.has_texture("bestscore/pill")) return;

    double f = page2_skipped ? 52.0 : 5.0 + std::max(0.0, on_page - totals_start) / FRAME_MS;
    if (f > 52.0) f = 52.0;
    if (f < 5.0) return;                          // not started yet

    static const float K[][3] = {
        {5, 24, 0}, {6, 7.45f, 0.36f}, {7, -5.4f, 0.64f}, {8, -14.6f, 0.84f},
        {9, -20.1f, 0.96f}, {10, -21.95f, 1}, {11, -20.55f, 1}, {12, -16.4f, 1},
        {13, -9.5f, 1}, {14, 0, 1}, {18, 7, 1}, {19, 4.5f, 1}, {20, 2.5f, 1},
        {21, 1.1f, 1}, {22, 0.3f, 1}, {23, 0, 1}, {52, 0, 1},
    };
    float ty = 0, a = 1;
    for (int i = 0; i + 1 < (int)(sizeof(K) / sizeof(K[0])); i++) {
        if (f >= K[i][0] && f <= K[i + 1][0]) {
            float u = (float)((f - K[i][0]) / (K[i + 1][0] - K[i][0]));
            ty = K[i][1] + (K[i + 1][1] - K[i][1]) * u;
            a  = K[i][2] + (K[i + 1][2] - K[i][2]) * u;
            break;
        }
    }

    tex.draw_texture(tex.get_enum("bestscore/pill"), {.y=ty, .fade=fade * a});

    // digits + (numbers appear f18..21)
    const double na = std::max(0.0, std::min(1.0, (f - 18.0) / 3.0));
    if (na > 0.0 && tex.has_texture("bestscore/num")) {
        TexID nt = tex.get_enum("bestscore/num");
        std::string s = std::to_string(rd.score - prev_best_score);
        for (int i = 0; i < (int)s.size(); i++) {
            const int digit = s[(int)s.size() - 1 - i] - '0';
            tex.draw_texture(nt, {.frame=digit, .x=-(float)(i * 14.17), .y=ty,
                                  .fade=fade * na});
        }
    }

    // glow burst f23..34
    if (f >= 23.0 && f < 34.0 && tex.has_texture("bestscore/glow")) {
        const float u = (float)((f - 23.0) / 11.0);
        tex.draw_texture(tex.get_enum("bestscore/glow"),
                         {.scale=1.0f + 0.6f * u, .center=true, .y=ty,
                          .fade=fade * (1.0f - u)});
    }
}

void DanResultScreen::draw_celebration(double now) {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;
    const double on_cel = now - celebrate_start_ms;
    const double fF = on_cel / FRAME_MS;

    const int r = rd.odai_result;
    const char* bg = (r == 2 || r == 5) ? "shoudan/bg_g"
                   : (r == 3 || r == 6) ? "shoudan/bg_r" : "shoudan/bg_w";
    if (tex.has_texture(bg))
        tex.draw_texture(tex.get_enum(bg), {});

    const char* text = r > 3 ? "shoudan/text_gold" : "shoudan/text_white";
    if (tex.has_texture(text)) {
        TexID tt = tex.get_enum(text);
        int ll = -1, lr = -1;
        const bool named = rd.dan_index >= 0 && shoudan_glyphs(rd.dan_index, ll, lr);
        struct Q { int frame; float x; double t0; };
        Q quads[4] = {{ll, 0.0f, 35.0}, {lr, 332.0f, 35.0},
                      {18, 664.0f, 63.0}, {19, 996.0f, 63.0}};
        for (const Q& q : quads) {
            if (q.frame < 0 || (!named && q.x < 600.0f)) continue;
            if (fF < q.t0) continue;
            const float pop = fF < q.t0 + 7.0
                ? 1.5f - 0.5f * (float)((fF - q.t0) / 7.0) : 1.0f;
            tex.draw_texture(tt, {.frame=q.frame, .scale=pop, .center=true, .x=q.x});
            const double e0 = q.t0 + 7.0;
            if (fF >= e0 && fF < e0 + 6.0) {
                const float u = (float)((fF - e0) / 6.0);
                tex.draw_texture(tt, {.frame=q.frame, .scale=1.0f + 0.25f * u,
                                      .center=true, .x=q.x, .fade=1.0 - u});
            }
            if (fF >= 110.0 && fF < 133.0) {
                const float u = (float)((fF - 110.0) / 23.0);
                tex.draw_texture(tt, {.frame=q.frame, .scale=1.0f + 0.96f * u,
                                      .center=true, .x=q.x, .fade=1.0 - u});
            }
        }
    }

    if (chara) chara->draw(927.0f, 923.0f);
    const double plate_fade = std::max(0.0, std::min(1.0, (fF - 21.0) / 8.0));
    if (on_cel >= 3000.0) {
        if (auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num))) {
            static int last_built_dan = -2;
            if (last_built_dan != pd->dan) {
                nameplate = Nameplate(pd->username, pd->title, global_data.player_num,
                                      pd->dan, pd->gold, pd->rainbow, pd->title_bg);
                last_built_dan = pd->dan;
            }
        }
    }
    if (plate_fade > 0.0) nameplate.draw(763.0f, 912.0f, plate_fade);
}

void DanResultScreen::draw_congrats(double now) {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;
    const double fF = (now - congrats_start_ms) / FRAME_MS;

    const int r = rd.odai_result;
    const char* sbg = (r == 2 || r == 5) ? "shoudan/bg_g"
                    : (r == 3 || r == 6) ? "shoudan/bg_r" : "shoudan/bg_w";
    if (tex.has_texture(sbg))
        tex.draw_texture(tex.get_enum(sbg), {});

    const double bg_fade = std::min(1.0, fF / 19.0);
    if (tex.has_texture("congrats/bg"))
        tex.draw_texture(tex.get_enum("congrats/bg"), {.fade=bg_fade});

    const char* text = (rd.dan_index == 24) ? "congrats/text_02" : "congrats/text_01";
    if (tex.has_texture(text)) {
        const std::string& lang = global_data.config->general.language;
        int frame = 0;
        if      (lang.rfind("en", 0) == 0) frame = 1;
        else if (lang.rfind("ko", 0) == 0) frame = 2;
        else if (lang.rfind("zh", 0) == 0) frame = 3;
        const double tf = std::max(0.0, std::min(1.0, (fF - 20.0) / 14.0));
        if (tf > 0.0)
            tex.draw_texture(tex.get_enum(text), {.frame=frame, .fade=tf});
    }

    if (chara) chara->draw(927.0f, 923.0f);
    nameplate.draw(763.0f, 912.0f);
}

void DanResultScreen::draw() {
    double now = get_current_ms();
    if (background.has_value()) background->draw();
    tex.draw_texture(BACKGROUND::DAN_RESULT_P1_BG, {});
    tex.draw_texture(BACKGROUND::DAN_RESULT_P1_BOARD, {});
    if (congrats_showing) {
        draw_congrats(now);
    } else if (celebrating) {
        draw_celebration(now);
    } else {
        draw_page1(now);
        draw_page2(page2_fade->attribute, now);
    }
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height,
                       ray::Fade(ray::BLACK, (float)fade_out->attribute));
    coin_overlay.draw();
    allnet_indicator.draw();
}
