#include "dan_result_draw.h"

#include <algorithm>

namespace {

    sol::table build_rd_table(const DanResultData& rd) {
        sol::state& lua = *script_manager.lua;
        sol::table t = lua.create_table();
        t["dan_color"] = rd.course.color;
        t["dan_rank"] = rd.course.rank;
        t["dan_index"] = rd.course.index;
        t["dan_index_max"] = rd.course.index_max;
        t["is_gaiden"] = rd.course.gaiden;
        t["dan_title"] = rd.dan_title;
        t["score"] = rd.score;
        t["gauge_length"] = rd.gauge_length;
        t["max_combo"] = rd.max_combo;
        t["odai_result"] = rd.odai_result;
        t["skipped"] = rd.skipped;
        t["player_num"] = static_cast<int>(global_data.player_num);

        sol::table songs = lua.create_table();
        for (size_t i = 0; i < rd.songs.size(); i++) {
            const DanResultSong& s = rd.songs[i];
            sol::table st = lua.create_table();
            st["selected_difficulty"] = s.selected_difficulty;
            st["diff_level"] = s.diff_level;
            st["song_title"] = s.song_title;
            st["genre_index"] = s.genre_index;
            st["good"] = s.good;
            st["ok"] = s.ok;
            st["bad"] = s.bad;
            st["drumroll"] = s.drumroll;
            st["hidden"] = s.hidden;
            st["unreached"] = s.unreached;
            songs[i + 1] = st;
        }
        t["songs"] = songs;

        sol::table exams = lua.create_table();
        for (size_t i = 0; i < rd.course.exams.size(); i++) {
            const Exam& e = rd.course.exams[i];
            sol::table et = lua.create_table();
            et["type"] = e.type;
            et["red"] = e.red;
            et["gold"] = e.gold;
            et["range"] = e.range;
            et["gothrough"] = e.gothrough;
            sol::table song_red = lua.create_table();
            for (size_t j = 0; j < e.song_red.size(); j++) {
                song_red[j + 1] = e.song_red[j];
            }
            et["song_red"] = song_red;
            exams[i + 1] = et;
        }
        t["exams"] = exams;

        sol::table exam_data = lua.create_table();
        for (size_t i = 0; i < rd.exam_data.size(); i++) {
            const DanResultExam& d = rd.exam_data[i];
            sol::table dt = lua.create_table();
            dt["progress"] = d.progress;
            dt["counter_value"] = d.counter_value;
            dt["failed"] = d.failed;
            sol::table sv = lua.create_table(), sp = lua.create_table(), ss = lua.create_table();
            for (size_t j = 0; j < std::size(d.song_value); j++) {
                sv[j + 1] = d.song_value[j];
                sp[j + 1] = d.song_progress[j];
                ss[j + 1] = d.song_state[j];
            }
            dt["song_value"] = sv;
            dt["song_progress"] = sp;
            dt["song_state"] = ss;
            dt["song_count"] = d.song_count;
            dt["tier"] = d.tier;
            dt["bar_state"] = d.bar_state;
            exam_data[i + 1] = dt;
        }
        t["exam_data"] = exam_data;

        return t;
    }

} // namespace

DanResultDraw::DanResultDraw(const DanResultData& rd, int prev_arrival, int prev_best_score, bool best_score_show,
                             int gauge_exam, int gauge_value, int gauge_border) {
    if (!script_manager.lua) {
        return;
    }
    if (!load("DanResultDraw", "dan_result_draw", build_rd_table(rd), prev_arrival, prev_best_score, best_score_show,
              gauge_exam, gauge_value, std::max(gauge_border, 1))) {
        return;
    }
    fn_draw = lua_object["draw"];
    fn_draw_overlay = lua_object["draw_overlay"];
    fn_chara_pos = lua_object["chara_pos"];
    fn_nameplate_pos = lua_object["nameplate_pos"];
}

sol::table DanResultDraw::state_table(const FrameState& s) {
    sol::state& lua = *script_manager.lua;
    sol::table t = lua.create_table();
    t["now"] = s.now;
    t["fade_out"] = s.fade_out_attr;
    t["page2_fade"] = s.page2_fade_attr;
    t["page_start_ms"] = s.page_start_ms;
    t["page1_start_ms"] = s.page1_start_ms;
    t["page2_skipped"] = s.page2_skipped;
    t["totals_start"] = s.totals_start;
    t["totals_end"] = s.totals_end;
    t["stamp_at"] = s.stamp_at;
    t["voice_at"] = s.voice_at;
    t["celebrating"] = s.celebrating;
    t["celebrate_start_ms"] = s.celebrate_start_ms;
    t["congrats_showing"] = s.congrats_showing;
    t["congrats_start_ms"] = s.congrats_start_ms;

    sol::table rows = lua.create_table();
    for (size_t i = 0; i < s.rows.size(); i++) {
        const DanResultRowSchedule& r = s.rows[i];
        sol::table rt = lua.create_table();
        rt["land"] = r.land;
        rt["fill0"] = r.fill0;
        rt["filld"] = r.filld;
        rt["numin"] = r.numin;
        rows[i + 1] = rt;
    }
    t["rows"] = rows;
    return t;
}

void DanResultDraw::draw(const FrameState& s) {
    if (!fn_draw.valid() || !script_manager.lua) {
        return;
    }
    call(fn_draw, "DanResultDraw:draw", state_table(s));
}

void DanResultDraw::draw_overlay(const FrameState& s) {
    if (!fn_draw_overlay.valid() || !script_manager.lua) {
        return;
    }
    call(fn_draw_overlay, "DanResultDraw:draw_overlay", state_table(s));
}

bool DanResultDraw::chara_pos(float& x, float& y, float& scale) {
    if (!fn_chara_pos.valid()) {
        return false;
    }
    auto pos_opt = call_r<sol::table>(fn_chara_pos, "DanResultDraw:chara_pos");
    if (!pos_opt) {
        return false;
    }
    sol::table& pos = pos_opt.value();
    sol::optional<float> px = pos[1], py = pos[2], ps = pos[3];
    if (!px || !py) {
        return false;
    }
    x = px.value();
    y = py.value();
    scale = ps.value_or(scale);
    return true;
}

bool DanResultDraw::nameplate_pos(float& x, float& y, float& fade) {
    if (!fn_nameplate_pos.valid()) {
        return false;
    }
    auto pos_opt = call_r<sol::table>(fn_nameplate_pos, "DanResultDraw:nameplate_pos");
    if (!pos_opt) {
        return false;
    }
    sol::table& pos = pos_opt.value();
    sol::optional<float> px = pos[1], py = pos[2], pf = pos[3];
    if (!px || !py) {
        return false;
    }
    x = px.value();
    y = py.value();
    fade = pf.value_or(fade);
    return true;
}
