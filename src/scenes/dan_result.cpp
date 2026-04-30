#include "dan_result.h"
#include "../libs/input.h"
#include "../libs/global_data.h"
#include <algorithm>

// ─── DanResultGauge ──────────────────────────────────────────────────────────

DanResultGauge::DanResultGauge(PlayerNum player_num, float gauge_len)
    : player_num(player_num), gauge_length(gauge_len)
    , visual_length(gauge_len * 8), is_rainbow(gauge_len == 89.0f) {}

void DanResultGauge::update(double current_ms) {
    if (!anims_loaded) {
        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
        rainbow_animation    = (TextureChangeAnimation*)tex.get_animation(64);
        rainbow_fade_in      = (FadeAnimation*)tex.get_animation(63);
        rainbow_animation->start();
        rainbow_fade_in->start();
        anims_loaded = true;
    }
    is_rainbow = (gauge_length == gauge_max);
    tamashii_fire_change->update(current_ms);
    rainbow_animation->update(current_ms);
    rainbow_fade_in->update(current_ms);
    if (rainbow_animation->is_finished) rainbow_animation->restart();
}

void DanResultGauge::draw(double fade) {
    if (!anims_loaded) return;
    tex.draw_texture(GAUGE::_1P_UNFILLED, {.fade=fade});

    if (!is_rainbow)
        tex.draw_texture(GAUGE::_1P_BAR, {.x2=visual_length-8, .fade=fade});

    if (is_rainbow) {
        float rf = std::min((float)rainbow_fade_in->attribute, (float)fade);
        if (rainbow_animation->attribute > 0 && rainbow_animation->attribute < 8)
            tex.draw_texture(GAUGE::RAINBOW, {.frame=(int)rainbow_animation->attribute-1, .fade=rf});
        tex.draw_texture(GAUGE::RAINBOW, {.frame=(int)rainbow_animation->attribute, .fade=std::min(rf, 0.75f)});
    }

    tex.draw_texture(GAUGE::OVERLAY, {.fade=std::min(fade, 0.15)});
    tex.draw_texture(GAUGE::FOOTER,  {.fade=fade});

    if (is_rainbow) {
        tex.draw_texture(GAUGE::TAMASHII_FIRE, {.frame=(int)tamashii_fire_change->attribute, .scale=0.75f, .center=true, .fade=fade});
        tex.draw_texture(GAUGE::TAMASHII,      {.fade=fade});
        int f = (int)tamashii_fire_change->attribute;
        if (f == 0 || f == 1 || f == 4 || f == 5)
            tex.draw_texture(GAUGE::TAMASHII_OVERLAY, {.fade=std::min(fade, 0.5)});
    } else {
        tex.draw_texture(GAUGE::TAMASHII_DARK, {.fade=fade});
    }
}

// ─── DanResultScreen ─────────────────────────────────────────────────────────

void DanResultScreen::on_screen_start() {
    Screen::on_screen_start();
    audio->play_sound("bgm", "music");

    fade_out   = (FadeAnimation*)tex.get_animation(0);
    page2_fade = (FadeAnimation*)tex.get_animation(1);
    is_page2   = false;

    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    background.emplace(PlayerNum::DAN, tex.screen_width);
    //gauge = std::make_unique<DanResultGauge>(global_data.player_num, sd.dan_result_data.gauge_length);

    int font_size = tex.skin_config[SC::DAN_TITLE].font_size;
    hori_name = std::make_unique<OutlinedText>(sd.dan_result_data.dan_title, font_size, ray::WHITE, ray::BLACK, false);

    song_names.clear();
    for (const auto& song : sd.dan_result_data.songs)
        song_names.push_back(std::make_unique<OutlinedText>(song.song_title, font_size, ray::WHITE, ray::BLACK, false));
}

Screens DanResultScreen::on_screen_end(Screens next_screen) {
    reset_session();
    return Screen::on_screen_end(next_screen);
}

void DanResultScreen::handle_input() {
    if (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)) {
        audio->play_sound("don", "sound");
        if (is_page2)
            fade_out->start();
        else {
            page2_fade->start();
            is_page2 = true;
        }
    }
}

std::optional<Screens> DanResultScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();

    handle_input();
    page2_fade->update(current_ms);
    fade_out->update(current_ms);
    //gauge->update(current_ms);

    if (fade_out->is_finished)
        return on_screen_end(Screens::DAN_SELECT);

    return std::nullopt;
}

void DanResultScreen::draw_digit_counter(const std::string& digits, float margin_x, TexID id,
                                          int index, float y, double fade, float scale) {
    for (int j = 0; j < (int)digits.size(); j++) {
        float x = -(float)(digits.size() - j) * margin_x;
        tex.draw_texture(id, {.frame=digits[j]-'0', .scale=scale, .x=x, .y=y, .fade=fade, .index=index});
    }
}

void DanResultScreen::draw_page1() {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    float height = tex.skin_config[SC::DAN_RESULT_INFO_HEIGHT].y;
    float margin = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].x;

    for (int i = 0; i < (int)sd.dan_result_data.songs.size(); i++) {
        const DanResultSong& song = sd.dan_result_data.songs[i];
        float y = i * height;

        tex.draw_texture(BACKGROUND::GENRE_BANNER, {.frame=song.genre_index, .y=y});

        if (i < (int)song_names.size() && song_names[i]) {
            SkinInfo sn = tex.skin_config[SC::DAN_RESULT_SONG_NAME];
            song_names[i]->draw({.x = sn.x - song_names[i]->width, .y = y + sn.y});
        }

        tex.draw_texture(RESULT_INFO::SONG_NUM,   {.frame=i, .y=y});
        tex.draw_texture(RESULT_INFO::DIFFICULTY, {.frame=song.selected_difficulty, .y=y});
        tex.draw_texture(RESULT_INFO::DIFF_STAR,  {.y=y});
        tex.draw_texture(RESULT_INFO::DIFF_X,     {.y=y});

        std::string lv = std::to_string(song.diff_level);
        float dm = tex.skin_config[SC::DAN_RESULT_DIFF_NUM_MARGIN].x;
        for (int j = 0; j < (int)lv.size(); j++)
            tex.draw_texture(RESULT_INFO::DIFF_NUM, {.frame=lv[j]-'0', .x=-(float)(j*dm), .y=y});

        auto draw_stat = [&](TexID icon, int val, int idx) {
            tex.draw_texture(icon, {.y=y});
            std::string sv = std::to_string(val);
            std::reverse(sv.begin(), sv.end());
            for (int j = 0; j < (int)sv.size(); j++)
                tex.draw_texture(RESULT_INFO::COUNTER, {.frame=sv[j]-'0', .x=-(float)(j*margin), .y=y, .index=idx});
        };
        draw_stat(RESULT_INFO::GOOD,     song.good,     0);
        draw_stat(RESULT_INFO::OK,       song.ok,       1);
        draw_stat(RESULT_INFO::BAD,      song.bad,      2);
        draw_stat(RESULT_INFO::DRUMROLL, song.drumroll, 3);
    }
}

void DanResultScreen::draw_exam_info(double fade, float scale) {
    const SessionData& sd  = global_data.session_data[(int)global_data.player_num];
    float offset_y = tex.skin_config[SC::DAN_EXAM_INFO].y * scale;
    float margin   = tex.skin_config[SC::DAN_EXAM_INFO].x * scale;

    for (int i = 0; i < (int)sd.dan_result_data.exams.size(); i++) {
        const Exam& exam       = sd.dan_result_data.exams[i];
        const DanResultExam& rd = sd.dan_result_data.exam_data[i];
        float y = i * offset_y;

        tex.draw_texture(EXAM_INFO::EXAM_BG,       {.scale=scale, .y=y, .fade=fade});
        tex.draw_texture(EXAM_INFO::EXAM_OVERLAY_1,{.scale=scale, .y=y, .fade=fade});

        float bar_w = tex.skin_config[SC::DAN_EXAM_INFO].width * rd.progress * scale;
        static const std::unordered_map<std::string, TexID> bar_ids = {
            {"exam_red",  EXAM_INFO::EXAM_RED},
            {"exam_gold", EXAM_INFO::EXAM_GOLD},
            {"exam_max",  EXAM_INFO::EXAM_MAX},
        };
        auto bar_it = bar_ids.find(rd.bar_texture);
        if (bar_it != bar_ids.end())
            tex.draw_texture(bar_it->second, {.scale=scale, .y=y, .x2=bar_w, .fade=fade});

        // Exam type icon
        static const std::unordered_map<std::string, TexID> icon_ids = {
            {"gauge",        EXAM_INFO::EXAM_GAUGE},
            {"combo",        EXAM_INFO::EXAM_COMBO},
            {"hit",          EXAM_INFO::EXAM_HIT},
            {"judgebad",     EXAM_INFO::EXAM_JUDGEBAD},
            {"judgegood",    EXAM_INFO::EXAM_JUDGEGOOD},
            {"judgeperfect", EXAM_INFO::EXAM_JUDGEPERFECT},
            {"score",        EXAM_INFO::EXAM_SCORE},
        };
        std::string red_str = std::to_string(exam.red);
        float type_x = -(float)red_str.size() * 20.0f * tex.screen_scale;
        auto icon_it = icon_ids.find(exam.type);
        if (icon_it != icon_ids.end())
            tex.draw_texture(icon_it->second, {.scale=scale, .x=type_x, .y=y, .fade=fade});

        draw_digit_counter(red_str, margin, EXAM_INFO::VALUE_COUNTER, 0, y, fade, scale);

        if (exam.range == "less")
            tex.draw_texture(EXAM_INFO::EXAM_LESS, {.scale=scale, .y=y, .fade=fade});
        else if (exam.range == "more")
            tex.draw_texture(EXAM_INFO::EXAM_MORE, {.scale=scale, .y=y, .fade=fade});

        tex.draw_texture(EXAM_INFO::EXAM_OVERLAY_2, {.scale=scale, .y=y, .fade=fade});
        std::string cur_str = std::to_string(rd.counter_value);
        draw_digit_counter(cur_str, margin, EXAM_INFO::VALUE_COUNTER, 1, y, fade, scale);

        if (exam.type == "gauge") {
            tex.draw_texture(EXAM_INFO::EXAM_PERCENT, {.scale=scale, .y=y, .fade=fade, .index=0});
            tex.draw_texture(EXAM_INFO::EXAM_PERCENT, {.scale=scale, .y=y, .fade=fade, .index=1});
        }

        if (rd.failed) {
            tex.draw_texture(EXAM_INFO::EXAM_BG,     {.scale=scale, .y=y, .fade=std::min(fade, 0.5)});
            tex.draw_texture(EXAM_INFO::EXAM_FAILED, {.scale=scale, .y=y, .fade=fade});
        }
    }
}

void DanResultScreen::draw_page2(double fade) {
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const DanResultData& rd = sd.dan_result_data;

    tex.draw_texture(BACKGROUND::RESULT_2_BG, {.fade=fade});
    for (int i = 0; i < 5; i++)
        tex.draw_texture(BACKGROUND::RESULT_2_DIVIDER, {.x=(float)(i*240), .fade=fade});
    tex.draw_texture(BACKGROUND::RESULT_2_PULLOUT, {.fade=fade});
    tex.draw_texture(RESULT_INFO::DAN_EMBLEM, {.frame=rd.dan_color, .fade=fade});

    if (hori_name) {
        SkinInfo hn = tex.skin_config[SC::DAN_RESULT_HORI_NAME];
        hori_name->draw({
            .x = hn.x - hori_name->width/2.0f,
            .y = hn.y,
            .x2= std::min(hori_name->width, hn.width) - hori_name->width,
            .fade=fade
        });
    }

    float margin = tex.skin_config[SC::SCORE_INFO_COUNTER_MARGIN].x;

    int total_good     = 0, total_ok = 0, total_bad = 0, total_dr = 0;
    for (const auto& s : rd.songs) {
        total_good += s.good; total_ok += s.ok; total_bad += s.bad; total_dr += s.drumroll;
    }

    auto draw_total = [&](TexID icon, int idx_icon, int total, int idx_counter) {
        tex.draw_texture(icon, {.fade=fade, .index=idx_icon});
        std::string sv = std::to_string(total); std::reverse(sv.begin(), sv.end());
        for (int j = 0; j < (int)sv.size(); j++)
            tex.draw_texture(RESULT_INFO::COUNTER, {.frame=sv[j]-'0', .x=-(float)(j*margin), .fade=fade, .index=idx_counter});
    };
    draw_total(RESULT_INFO::GOOD,     1, total_good, 4);
    draw_total(RESULT_INFO::OK,       1, total_ok,   5);
    draw_total(RESULT_INFO::BAD,      1, total_bad,  6);
    draw_total(RESULT_INFO::DRUMROLL, 1, total_dr,   7);

    tex.draw_texture(RESULT_INFO::MAX_COMBO, {.fade=fade});
    { std::string sv = std::to_string(rd.max_combo); std::reverse(sv.begin(), sv.end());
      for (int j = 0; j < (int)sv.size(); j++)
          tex.draw_texture(RESULT_INFO::COUNTER, {.frame=sv[j]-'0', .x=-(float)(j*margin), .fade=fade, .index=8}); }

    int total_hits = total_good + total_ok + total_bad + total_dr;
    tex.draw_texture(RESULT_INFO::MAX_HITS, {.fade=fade});
    { std::string sv = std::to_string(total_hits); std::reverse(sv.begin(), sv.end());
      for (int j = 0; j < (int)sv.size(); j++)
          tex.draw_texture(RESULT_INFO::COUNTER, {.frame=sv[j]-'0', .x=-(float)(j*margin), .fade=fade, .index=9}); }

    tex.draw_texture(RESULT_INFO::EXAM_HEADER, {.fade=fade});

    // Score box
    tex.draw_texture(RESULT_INFO::SCORE_BOX, {.fade=fade});
    float sm = tex.skin_config[SC::DAN_SCORE_BOX_MARGIN].x;
    { std::string sv = std::to_string(rd.score); std::reverse(sv.begin(), sv.end());
      for (int j = 0; j < (int)sv.size(); j++)
          tex.draw_texture(RESULT_INFO::SCORE_COUNTER, {.frame=sv[j]-'0', .x=-(float)(j*sm), .fade=fade}); }

    //gauge->draw(fade);
    draw_exam_info(fade);

    // Pass / Fail verdict
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

void DanResultScreen::draw() {
    if (background.has_value()) background->draw();
    draw_page1();
    draw_page2(page2_fade->attribute);
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height,
                       ray::Fade(ray::BLACK, (float)fade_out->attribute));
    coin_overlay.draw();
    allnet_indicator.draw();
}
