#include "box_dan.h"
TextureObject* exam_icon_id(const std::string& preferred, const char* folder);
#include "../../../libs/song_parser.h"
#include "../../../libs/scores.h"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

DanBox::DanBox(const fs::path& path, const std::string& title, int color,
               const std::vector<DanSongEntry>& songs_in,
               const std::vector<Exam>& exams_in, int total_notes_in)
    : BaseBox(path, BoxDef{title, static_cast<TextureIndex>(color), GenreIndex::DAN, "", "", std::nullopt, std::nullopt})
    , dan_title(title), dan_color(color)
    , songs(songs_in), exams(exams_in), total_notes(total_notes_in)
{
    text_name = title;

    load_textures();
}

void DanBox::load_textures() {
    BaseBox::load_textures();
    t_dan_folder = tex.get_texture("box/folder");
    t_genre_banner = tex.get_texture("yellow_box/genre_banner");
    t_song_label = tex.get_texture("yellow_box/song_label");
    t_difficulty = tex.get_texture("yellow_box/difficulty");
    t_difficulty_x = tex.get_texture("yellow_box/difficulty_x");
    t_difficulty_star = tex.get_texture("yellow_box/difficulty_star");
    t_difficulty_num = tex.get_texture("yellow_box/difficulty_num");
    t_total_notes_bg = tex.get_texture("yellow_box/total_notes_bg");
    t_total_notes = tex.get_texture("yellow_box/total_notes");
    t_total_notes_counter = tex.get_texture("yellow_box/total_notes_counter");
    t_rank_plate = tex.get_texture("yellow_box/rank_plate");
    t_dan_rank_frame = tex.get_texture("yellow_box/frame");
    t_exam_box_bottom_right = tex.get_texture("yellow_box/exam_box_bottom_right");
    t_exam_box_bottom_left = tex.get_texture("yellow_box/exam_box_bottom_left");
    t_exam_box_top_right = tex.get_texture("yellow_box/exam_box_top_right");
    t_exam_box_top_left = tex.get_texture("yellow_box/exam_box_top_left");
    t_exam_box_bottom = tex.get_texture("yellow_box/exam_box_bottom");
    t_exam_box_right = tex.get_texture("yellow_box/exam_box_right");
    t_exam_box_left = tex.get_texture("yellow_box/exam_box_left");
    t_exam_box_top = tex.get_texture("yellow_box/exam_box_top");
    t_exam_box_center = tex.get_texture("yellow_box/exam_box_center");
    t_exam_header = tex.get_texture("yellow_box/exam_header");
    t_judge_box = tex.get_texture("yellow_box/judge_box");
    t_exam_percent = tex.get_texture("yellow_box/exam_percent");
    t_judge_num = tex.get_texture("yellow_box/judge_num");
    t_exam_more = tex.get_texture("yellow_box/exam_more");
    t_exam_less = tex.get_texture("yellow_box/exam_less");
    t_exam_frame = tex.get_texture("yellow_box/exam_frame");

    static const std::unordered_map<std::string, std::string> exam_icon_paths = {
        {"gauge",        "yellow_box/exam_gauge"},
        {"combo",        "yellow_box/exam_combo"},
        {"hit",          "yellow_box/exam_hit"},
        {"judgebad",     "yellow_box/exam_judgebad"},
        {"judgegood",    "yellow_box/exam_judgegood"},
        {"judgeperfect", "yellow_box/exam_judgeperfect"},
        {"score",        "yellow_box/exam_score"},
        {"renda",        "yellow_box/exam_roll"},
    };
    for (const auto& [type, path] : exam_icon_paths)
        t_exam_icon_by_type[type] = exam_icon_id(path, "yellow_box");
}

static float dan_shrink_font(float font_size) {
    return std::max(font_size - 10.0f * tex.screen_scale, font_size * 0.6f);
}

void DanBox::load_text() {
    BaseBox::load_text();  // populates name (vertical) for closed state
    const SkinInfo* chip_slot = tex.skin_entry("dan_chip_name");
    float base_font = (chip_slot && chip_slot->font_size > 0)
                    ? (float)chip_slot->font_size
                    : (float)tex.skin_config[SC::SONG_BOX_NAME].font_size;
    float name_outline = 5.0f;
    if (utf8_char_count(text_name) >= 30) {
        float shrunk = dan_shrink_font(base_font);
        name_outline = 5.0f * (shrunk / base_font);
        base_font = shrunk;
    }
    const bool chip_black = tex.options[SCO::DAN_CHIP_NAME_BLACK];
    name = std::make_unique<OutlinedText>(text_name, (int)base_font,
                                          chip_black ? ray::BLACK : ray::WHITE,
                                          ray::BLACK, true,
                                          chip_black ? 0.0f : name_outline);
    int font_size = tex.skin_config[SC::DAN_TITLE].font_size;
    hori_name = std::make_unique<OutlinedText>(dan_title, font_size, ray::WHITE, ray::BLACK, false);

    int revealed = 0;
    {
        bool any_hidden = false;
        for (const auto& e : songs) any_hidden |= e.hidden;
        if (any_hidden) {
            auto rec = scores_manager.get_dan_record(
                get_player_id(global_data.player_num), dan_title);
            if (rec && rec->rank > 0) revealed = rec->arrival;
        }
    }

    const std::string& lang = global_data.config->general.language;
    int song_idx = 0;
    const bool have_titles = song_titles.size() == songs.size();
    for (auto& entry : songs) {
        std::string title_str, sub_str;
        if (have_titles) {
            title_str = song_titles[song_idx].first;
            sub_str   = song_titles[song_idx].second;
        } else {
            SongParser sp(entry.song_path);
            title_str = sp.metadata.title.count(lang) ? sp.metadata.title.at(lang) : sp.metadata.title.at("en");
            sub_str   = sp.metadata.subtitle.count(lang) ? sp.metadata.subtitle.at(lang) : "";
        }
        if (entry.hidden && song_idx >= revealed) {
            title_str = "？？？";
            sub_str.clear();
        }
        song_idx++;

        int base_sub_font = tex.skin_config[SC::DAN_SUBTITLE].font_size;
        int sub_font = base_sub_font;
        float sub_outline = 5.0f;
        if (sub_str.size() >= 30) {
            float shrunk = dan_shrink_font((float)base_sub_font);
            sub_outline = 5.0f * (shrunk / (float)base_sub_font);
            sub_font = (int)shrunk;
        }

        const bool vertical = !tex.options[SCO::DAN_TITLE_HORIZONTAL];
        song_texts.push_back({
            std::make_unique<OutlinedText>(title_str, font_size, ray::WHITE, ray::BLACK, vertical),
            std::make_unique<OutlinedText>(sub_str,   sub_font,  ray::WHITE, ray::BLACK, vertical, sub_outline)
        });
    }
    text_loaded = true;
}

void DanBox::update(double current_ms) {
    BaseBox::update(current_ms);
    if (yellow_box.has_value() && yellow_box_opened && !yellow_box->is_diff_select)
        yellow_box->create_anim_2();
}

void DanBox::draw_chip() {
    tex.draw_texture(t_shadow_bottom_left,  {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(t_shadow_bottom,       {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(t_shadow_bottom_right, {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(t_shadow_right,        {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(t_shadow_top_right,    {.x=position, .fade=fade->attribute, .index=0});

    tex.draw_texture(t_dan_folder, {.frame=dan_color, .x=position, .fade=fade->attribute});

    if (text_loaded && name) {
        const SkinInfo* chip = tex.skin_entry("dan_chip_name");
        const SkinInfo& nb = chip ? *chip : tex.skin_config[SC::SONG_BOX_NAME];
        name->draw({
            .x = position + nb.x - name->width / 2.0f,
            .y = nb.y,
            .y2 = std::min(name->height, nb.height) - name->height,
            .fade = fade->attribute
        });
    }
}

void DanBox::draw_closed() { draw_chip(); }

void DanBox::draw_open() {
    if (!yellow_box.has_value()) return;
    draw_chip();
    yellow_box->draw();

    if (!text_loaded) return;
    float f = open_fade->attribute;

    float offset_x = tex.skin_config[SC::DAN_YELLOW_BOX_OFFSET].x;
    float offset_y = tex.skin_config[SC::DAN_YELLOW_BOX_OFFSET].y;
    for (int i = 0; i < (int)songs.size(); i++) {
        float x = i * offset_x;
        float y = i * offset_y;
        tex.draw_texture(t_genre_banner,   {.frame=songs[i].genre_index, .x=x, .y=y, .fade=f});
        if (tex.has_texture("yellow_box/song_label"))
            tex.draw_texture(t_song_label,
                             {.frame=std::min(i, 2), .x=x, .y=y, .fade=f});
        tex.draw_texture(t_difficulty,     {.frame=songs[i].difficulty,  .x=x, .y=y, .fade=f});
        tex.draw_texture(t_difficulty_x,   {.x=x, .y=y, .fade=f});
        tex.draw_texture(t_difficulty_star,{.x=x, .y=y, .fade=f});

        // Level counter
        std::string lvl = std::to_string(songs[i].level);
        float margin = tex.skin_config[SC::DAN_LEVEL_COUNTER_MARGIN].x;
        float total_w = lvl.size() * margin;
        for (int j = 0; j < (int)lvl.size(); j++) {
            tex.draw_texture(t_difficulty_num, {.frame=lvl[j]-'0', .x=x-(total_w/2)+(j*margin), .y=y, .fade=f});
        }

        // Song title and subtitle
        if (i < (int)song_texts.size()) {
            auto& [title_text, sub_text] = song_texts[i];
            SkinInfo td = tex.skin_config[SC::DAN_TITLE];
            SkinInfo sd = tex.skin_config[SC::DAN_SUBTITLE];
            if (title_text)
                title_text->draw({.x=td.x+x, .y=td.y+y, .y2=std::min(title_text->height, td.height)-title_text->height, .fade=f});
            if (sub_text)
                sub_text->draw({.x=sd.x+x, .y=sd.y+y-std::min(sub_text->height, sd.height), .y2=std::min(sub_text->height, sd.height)-sub_text->height, .fade=f});
        }
    }

    tex.draw_texture(t_total_notes_bg, {.fade=f});
    const SkinInfo* tn_slot = tex.skin_entry("dan_select_total_notes");
    if (!tn_slot || tn_slot->x >= 1.0f) {
        tex.draw_texture(t_total_notes,    {.fade=f});
        std::string tn = std::to_string(total_notes);
        float tn_margin = tex.skin_config[SC::TOTAL_NOTES_COUNTER_MARGIN].x;
        for (int i = 0; i < (int)tn.size(); i++)
            tex.draw_texture(t_total_notes_counter, {.frame=tn[i]-'0', .x=(float)(i*tn_margin), .fade=f});
    }

    if (dan_rank >= 0 && tex.options[SCO::DAN_SELECT_RANK_PLATE]) {
        tex.draw_texture(t_rank_plate, {.frame=dan_rank, .fade=f});
    } else {
        tex.draw_texture(t_dan_rank_frame, {.frame=dan_color, .fade=f});
        if (hori_name) {
            SkinInfo hn = tex.skin_config[SC::DAN_HORI_NAME];
            hori_name->draw({
                .x = hn.x - hori_name->width/2.0f,
                .y = hn.y,
                .x2 = std::min(hori_name->width, hn.width) - hori_name->width,
                .fade = f
            });
        }
    }

    draw_exam_box();
}

static void draw_abs(const std::string& name, float X, float Y, float fade,
                     int frame = 0) {
    if (!tex.has_texture(name)) return;
    TextureObject* t = tex.get_texture(name);
    if (t->x.empty()) return;
    tex.draw_texture(t, {.frame = frame,
                          .x = X - t->x[0],
                          .y = Y - t->y[0],
                          .fade = fade});
}

void DanBox::draw_exam_grid() {
    const float f = open_fade->attribute;

    if (tex.has_texture("yellow_box/exam_frame"))
        tex.draw_texture(t_exam_frame, {.fade=f});

    tex.draw_texture(t_exam_header, {.fade=f});

    auto num = [&](const SkinInfo* s, float dx, float dy) {
        return std::pair<float,float>{s ? s->x : dx, s ? s->y : dy};
    };
    const SkinInfo* gb = tex.skin_entry("dan_exam_gauge_box");
    const SkinInfo* sb = tex.skin_entry("dan_exam_score_box");
    auto [gx, gy] = num(gb, 436.0f, 615.0f);
    auto [sx, sy] = num(sb, 769.0f, 615.0f);
    const float pitch  = (sb && sb->height > 0) ? sb->height : 132.0f;
    const SkinInfo* sg = tex.skin_entry("dan_exam_seg");
    const float seg_x  = sg ? sg->x     : 14.0f;
    const float seg_y  = sg ? sg->y     : 48.0f;
    const float seg_dx = (sg && sg->width > 0) ? sg->width : 327.0f;
    const SkinInfo* vp = tex.skin_entry("dan_exam_value");
    const float val_x  = vp ? vp->x : 185.0f;
    const float val_y  = vp ? vp->y : 40.0f;
    const SkinInfo* pp = tex.skin_entry("dan_exam_label_pill");
    const float pill_x = pp ? pp->x : 26.0f;
    const float pill_y = pp ? pp->y : 2.0f;

    static const std::unordered_map<std::string, std::string> exam_icons = {
        {"gauge",        "yellow_box/exam_gauge"},
        {"combo",        "yellow_box/exam_combo"},
        {"hit",          "yellow_box/exam_hit"},
        {"judgebad",     "yellow_box/exam_judgebad"},
        {"judgegood",    "yellow_box/exam_judgegood"},
        {"judgeperfect", "yellow_box/exam_judgeperfect"},
        {"score",        "yellow_box/exam_score"},
        {"renda",        "yellow_box/exam_roll"},
    };

    const float margin = tex.skin_config[SC::EXAM_COUNTER_MARGIN].x;

    const SkinInfo* seg_bt   = tex.skin_entry("dan_select_exam_border_text");
    const SkinInfo* gauge_bt = tex.skin_entry("dan_select_gauge_border_text");

    auto draw_value_text = [&](const Exam& exam, const SkinInfo* bt,
                               float right_x, float top_y) -> bool {
        if (!bt) return false;
        const float ol = bt->outline >= 0 ? bt->outline : (3.0f / tex.screen_scale);
        OutlinedText* cap = exam_captions.get(
            exam_border_text(tex, exam, global_data.config->general.language),
            bt->font_size > 0 ? bt->font_size : 28, ol);
        if (!cap) return false;
        const float pad = ExamCaptionCache::pad_for(ol, tex.screen_scale);
        cap->draw({.x = right_x - cap->width + pad, .y = top_y - pad, .fade = f});
        return true;
    };

    auto draw_value = [&](const Exam& exam, float centre_x, float top_y) {
        const std::string digits = std::to_string(exam.red);
        float pct_w = 0.0f, suf_w = 0.0f;
        if (exam.type == "gauge" && tex.has_texture("yellow_box/exam_percent"))
            pct_w = (float)tex.get_texture("yellow_box/exam_percent")->width;
        const char* suffix = exam.range == "less" ? "yellow_box/exam_less"
                           : exam.range == "more" ? "yellow_box/exam_more" : nullptr;
        if (suffix && tex.has_texture(suffix))
            suf_w = (float)tex.get_texture(suffix)->width;
        const float total = digits.size() * margin + pct_w + suf_w;
        float cx = centre_x - total * 0.5f;
        for (char c : digits) {
            draw_abs("yellow_box/judge_num", cx, top_y, f, c - '0');
            cx += margin;
        }
        if (pct_w > 0.0f) { draw_abs("yellow_box/exam_percent", cx, top_y + 8, f); cx += pct_w; }
        if (suf_w > 0.0f) { draw_abs(suffix, cx, top_y + 6, f); }
    };

    int right_row = 0;
    bool gauge_used = false;
    for (const Exam& exam : exams) {
        const bool left = (exam.type == "gauge" && !gauge_used);
        if (left) gauge_used = true;
        if (!left && right_row >= 3) continue;      // the cabinet has three slots

        const float BX = left ? gx : sx;
        const float BY = left ? gy : (sy + pitch * right_row);

        draw_abs(left ? "yellow_box/exam_gauge_box" : "yellow_box/judge_box",
                 BX - (left ? 0.0f : 3.0f), BY - (left ? 0.0f : 2.0f), f);

        auto ic = exam_icons.find(exam.type);
        if (ic != exam_icons.end())
            draw_abs(ic->second, BX + pill_x, BY + pill_y, f);

        if (left) {
            if (!draw_value_text(exam, gauge_bt,
                                 BX + (gauge_bt ? gauge_bt->x : 0.0f),
                                 BY + (gauge_bt ? gauge_bt->y : 0.0f)))
                draw_value(exam, BX + 181.0f, BY + 45.0f);
        } else {
            const int segs = exam.gothrough ? 1 : (int)std::min<size_t>(songs.size(), 3);
            for (int k = 0; k < segs; k++) {
                const int frame = exam.gothrough ? 0 : (k + 1);
                draw_abs("yellow_box/exam_seg_label",
                         BX + seg_x + seg_dx * k, BY + seg_y, f, frame);
                const Exam seg_exam = exam.for_song(k);   // per-song borders: this song's pair
                if (!draw_value_text(seg_exam, seg_bt,
                                     BX + (seg_bt ? seg_bt->x : 0.0f) + seg_dx * k,
                                     BY + (seg_bt ? seg_bt->y : 0.0f)))
                    draw_value(seg_exam, BX + val_x + seg_dx * k, BY + val_y);
            }
        }
        if (!left) right_row++;
    }
}

void DanBox::draw_exam_box() {
    if (tex.options[SCO::DAN_EXAM_GRID]) { draw_exam_grid(); return; }

    float f = open_fade->attribute;
    tex.draw_texture(t_exam_box_bottom_right, {.fade=f});
    tex.draw_texture(t_exam_box_bottom_left,  {.fade=f});
    tex.draw_texture(t_exam_box_top_right,    {.fade=f});
    tex.draw_texture(t_exam_box_top_left,     {.fade=f});
    tex.draw_texture(t_exam_box_bottom,       {.fade=f});
    tex.draw_texture(t_exam_box_right,        {.fade=f});
    tex.draw_texture(t_exam_box_left,         {.fade=f});
    tex.draw_texture(t_exam_box_top,          {.fade=f});
    tex.draw_texture(t_exam_box_center,       {.fade=f});
    tex.draw_texture(t_exam_header,           {.fade=f});

    float offset_y = tex.skin_config[SC::DAN_EXAM_INFO].y;
    float margin   = tex.skin_config[SC::EXAM_COUNTER_MARGIN].x;

    // Four or more conditions do not fit the board: drop the soul-gauge row (it is the
    // one condition every course carries and the gauge itself shows it in game).
    std::vector<const Exam*> shown;
    for (const Exam& e : exams)
        if (!(exams.size() >= 4 && e.type == "gauge")) shown.push_back(&e);
    if ((int)shown.size() > 3) shown.resize(3);
    for (int i = 0; i < (int)shown.size(); i++) {
        const Exam& exam = *shown[i];
        float y = i * offset_y;
        tex.draw_texture(t_judge_box, {.y=y, .fade=f});

        // Exam type icon
        auto icon_it = t_exam_icon_by_type.find(exam.type);
        if (icon_it != t_exam_icon_by_type.end())
            tex.draw_texture(icon_it->second, {.y=y, .fade=f});

        // Optional skin key dan_exam_value_center: centre the whole value (digits, %, suffix) on
        // its x, with `width` px between the parts. Skins without the key keep the per-part
        // texture positions below, as before.
        if (const SkinInfo* vc = tex.skin_entry("dan_exam_value_center")) {
            const std::string digits = std::to_string(exam.red);
            TextureObject* pct = exam.type == "gauge" ? t_exam_percent : nullptr;
            TextureObject* suf = exam.range == "more" ? t_exam_more
                               : exam.range == "less" ? t_exam_less : nullptr;
            const float digits_w = (digits.size() - 1) * margin + t_judge_num->x2[0];
            const float gap = vc->width;
            const int parts = 1 + (pct ? 1 : 0) + (suf ? 1 : 0);
            const float total = digits_w + (pct ? pct->x2[0] : 0.0f) + (suf ? suf->x2[0] : 0.0f)
                              + gap * (parts - 1);
            float cx = vc->x - total / 2.0f;
            for (size_t j = 0; j < digits.size(); j++)
                tex.draw_texture(t_judge_num, {.frame=digits[j]-'0', .x=cx + j * margin - t_judge_num->x[0], .y=y, .fade=f});
            cx += digits_w + gap;
            if (pct) { tex.draw_texture(pct, {.x=cx - pct->x[0], .y=y, .fade=f}); cx += pct->x2[0] + gap; }
            if (suf) tex.draw_texture(suf, {.x=cx - suf->x[0], .y=y, .fade=f});
            continue;
        }

        float x_offset = 0;
        if (exam.type == "gauge") {
            tex.draw_texture(t_exam_percent, {.y=y, .fade=f});
            x_offset = tex.skin_config[SC::EXAM_GAUGE_OFFSET].x;
        }

        std::string counter = std::to_string(exam.red);
        for (int j = 0; j < (int)counter.size(); j++) {
            float x = x_offset - (counter.size() - j) * margin;
            tex.draw_texture(t_judge_num, {.frame=counter[j]-'0', .x=x, .y=y, .fade=f});
        }

        if (exam.range == "more")
            tex.draw_texture(t_exam_more, {.x=-x_offset*1.7f, .y=y, .fade=f});
        else if (exam.range == "less")
            tex.draw_texture(t_exam_less, {.x=-x_offset*1.7f, .y=y, .fade=f});
    }
}
