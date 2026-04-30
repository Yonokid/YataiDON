#include "box_dan.h"
#include "../../../libs/parsers/song_parser.h"

DanBox::DanBox(const fs::path& path, const std::string& title, int color,
               const std::vector<DanSongEntry>& songs_in,
               const std::vector<Exam>& exams_in, int total_notes_in)
    : BaseBox(path, BoxDef{title, static_cast<TextureIndex>(color), GenreIndex::DAN, "", std::nullopt, std::nullopt})
    , dan_title(title), dan_color(color)
    , songs(songs_in), exams(exams_in), total_notes(total_notes_in)
{
    text_name = title;
}

void DanBox::load_text() {
    BaseBox::load_text();  // populates name (vertical) for closed state
    float base_font = tex.skin_config[SC::SONG_BOX_NAME].font_size;
    if (utf8_char_count(text_name) >= 30) base_font -= (int)(10 * tex.screen_scale);
    name = std::make_unique<OutlinedText>(text_name, (int)base_font, ray::WHITE, ray::BLACK, true);
    int font_size = tex.skin_config[SC::DAN_TITLE].font_size;
    hori_name = std::make_unique<OutlinedText>(dan_title, font_size, ray::WHITE, ray::BLACK, false);

    const std::string& lang = global_data.config->general.language;
    for (auto& entry : songs) {
        SongParser sp(entry.song_path);
        std::string title_str = sp.metadata.title.count(lang) ? sp.metadata.title.at(lang) : sp.metadata.title.at("en");
        std::string sub_str   = sp.metadata.subtitle.count(lang) ? sp.metadata.subtitle.at(lang) : "";

        int sub_font = tex.skin_config[SC::DAN_SUBTITLE].font_size;
        if (sub_str.size() >= 30)
            sub_font -= (int)(10 * tex.screen_scale);

        song_texts.push_back({
            std::make_unique<OutlinedText>(title_str, font_size, ray::WHITE, ray::BLACK, true),
            std::make_unique<OutlinedText>(sub_str,   sub_font,  ray::WHITE, ray::BLACK, true)
        });
    }
    text_loaded = true;
}

void DanBox::update(double current_ms) {
    BaseBox::update(current_ms);
    if (yellow_box.has_value() && yellow_box_opened && !yellow_box->is_diff_select)
        yellow_box->create_anim_2();
}

void DanBox::draw_closed() {
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_LEFT,  {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM,       {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_RIGHT, {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(YELLOW_BOX::SHADOW_RIGHT,        {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture(YELLOW_BOX::SHADOW_TOP_RIGHT,    {.x=position, .fade=fade->attribute, .index=0});

    tex.draw_texture(BOX::FOLDER, {.frame=dan_color, .x=position, .fade=fade->attribute});

    if (text_loaded && name) {
        name->draw({
            .x = position + tex.skin_config[SC::SONG_BOX_NAME].x - name->width / 2.0f,
            .y = tex.skin_config[SC::SONG_BOX_NAME].y,
            .y2 = std::min(name->height, tex.skin_config[SC::SONG_BOX_NAME].height) - name->height,
            .fade = fade->attribute
        });
    }
}

void DanBox::draw_open() {
    if (!yellow_box.has_value()) return;
    yellow_box->draw();

    if (!text_loaded) return;
    float f = open_fade->attribute;

    float offset_x = tex.skin_config[SC::DAN_YELLOW_BOX_OFFSET].x;
    for (int i = 0; i < (int)songs.size(); i++) {
        float x = i * offset_x;
        tex.draw_texture(YELLOW_BOX::GENRE_BANNER,   {.frame=songs[i].genre_index, .x=x, .fade=f});
        tex.draw_texture(YELLOW_BOX::DIFFICULTY,     {.frame=songs[i].difficulty,  .x=x, .fade=f});
        tex.draw_texture(YELLOW_BOX::DIFFICULTY_X,   {.x=x, .fade=f});
        tex.draw_texture(YELLOW_BOX::DIFFICULTY_STAR,{.x=x, .fade=f});

        // Level counter
        std::string lvl = std::to_string(songs[i].level);
        float margin = tex.skin_config[SC::DAN_LEVEL_COUNTER_MARGIN].x;
        float total_w = lvl.size() * margin;
        for (int j = 0; j < (int)lvl.size(); j++) {
            tex.draw_texture(YELLOW_BOX::DIFFICULTY_NUM, {.frame=lvl[j]-'0', .x=x-(total_w/2)+(j*margin), .fade=f});
        }

        // Song title and subtitle
        if (i < (int)song_texts.size()) {
            auto& [title_text, sub_text] = song_texts[i];
            SkinInfo td = tex.skin_config[SC::DAN_TITLE];
            SkinInfo sd = tex.skin_config[SC::DAN_SUBTITLE];
            if (title_text)
                title_text->draw({.x=td.x+x, .y=td.y, .y2=std::min(title_text->height, td.height)-title_text->height, .fade=f});
            if (sub_text)
                sub_text->draw({.x=sd.x+x, .y=sd.y-std::min(sub_text->height, sd.height), .y2=std::min(sub_text->height, sd.height)-sub_text->height, .fade=f});
        }
    }

    // Total notes
    tex.draw_texture(YELLOW_BOX::TOTAL_NOTES_BG, {.fade=f});
    tex.draw_texture(YELLOW_BOX::TOTAL_NOTES,    {.fade=f});
    std::string tn = std::to_string(total_notes);
    float tn_margin = tex.skin_config[SC::TOTAL_NOTES_COUNTER_MARGIN].x;
    for (int i = 0; i < (int)tn.size(); i++)
        tex.draw_texture(YELLOW_BOX::TOTAL_NOTES_COUNTER, {.frame=tn[i]-'0', .x=(float)(i*tn_margin), .fade=f});

    // Frame
    tex.draw_texture(YELLOW_BOX::FRAME, {.frame=dan_color, .fade=f});
    if (hori_name) {
        SkinInfo hn = tex.skin_config[SC::DAN_HORI_NAME];
        hori_name->draw({
            .x = hn.x - hori_name->width/2.0f,
            .y = hn.y,
            .x2 = std::min(hori_name->width, hn.width) - hori_name->width,
            .fade = f
        });
    }

    draw_exam_box();
}

void DanBox::draw_exam_box() {
    float f = open_fade->attribute;
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_BOTTOM_RIGHT, {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_BOTTOM_LEFT,  {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_TOP_RIGHT,    {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_TOP_LEFT,     {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_BOTTOM,       {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_RIGHT,        {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_LEFT,         {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_TOP,          {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_BOX_CENTER,       {.fade=f});
    tex.draw_texture(YELLOW_BOX::EXAM_HEADER,           {.fade=f});

    float offset_y = tex.skin_config[SC::DAN_EXAM_INFO].y;
    float margin   = tex.skin_config[SC::EXAM_COUNTER_MARGIN].x;

    for (int i = 0; i < (int)exams.size(); i++) {
        const Exam& exam = exams[i];
        float y = i * offset_y;
        tex.draw_texture(YELLOW_BOX::JUDGE_BOX, {.y=y, .fade=f});

        // Exam type icon
        static const std::unordered_map<std::string, TexID> exam_icons = {
            {"gauge",        YELLOW_BOX::EXAM_GAUGE},
            {"combo",        YELLOW_BOX::EXAM_COMBO},
            {"hit",          YELLOW_BOX::EXAM_HIT},
            {"judgebad",     YELLOW_BOX::EXAM_JUDGEBAD},
            {"judgegood",    YELLOW_BOX::EXAM_JUDGEGOOD},
            {"judgeperfect", YELLOW_BOX::EXAM_JUDGEPERFECT},
            {"score",        YELLOW_BOX::EXAM_SCORE},
        };
        auto icon_it = exam_icons.find(exam.type);
        if (icon_it != exam_icons.end())
            tex.draw_texture(icon_it->second, {.y=y, .fade=f});

        float x_offset = 0;
        if (exam.type == "gauge") {
            tex.draw_texture(YELLOW_BOX::EXAM_PERCENT, {.y=y, .fade=f});
            x_offset = tex.skin_config[SC::EXAM_GAUGE_OFFSET].x;
        }

        std::string counter = std::to_string(exam.red);
        for (int j = 0; j < (int)counter.size(); j++) {
            float x = x_offset - (counter.size() - j) * margin;
            tex.draw_texture(YELLOW_BOX::JUDGE_NUM, {.frame=counter[j]-'0', .x=x, .y=y, .fade=f});
        }

        if (exam.range == "more")
            tex.draw_texture(YELLOW_BOX::EXAM_MORE, {.x=-x_offset*1.7f, .y=y, .fade=f});
        else if (exam.range == "less")
            tex.draw_texture(YELLOW_BOX::EXAM_LESS, {.x=-x_offset*1.7f, .y=y, .fade=f});
    }
}
