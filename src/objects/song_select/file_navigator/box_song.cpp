#include "box_song.h"

SongBox::SongBox(const fs::path& path, const BoxDef& box_def, TJAParser parser)
    : BaseBox(path, box_def)
{
    this->parser = parser;
    parser.get_metadata();
    auto& titles = parser.metadata.title;
    const std::string& lang = global_data.config->general.language;
    text_name = titles.count(lang) ? titles.at(lang) : titles.at("en");

    auto& subtitles = parser.metadata.subtitle;
    text_subtitle = subtitles.count(lang) ? subtitles.at(lang) : subtitles.at("en");

    is_favorite = false;
    diff_fade_in = (FadeAnimation*)tex.get_animation(12);
    hashes = scores_manager.get_hashes(path);
    for (int i = 0; i < 5; i++) {
        scores[i] = scores_manager.get_score(hashes[i], i, 1);
    }
}

void SongBox::reset() {
    BaseBox::reset();
    diff_fade_in = (FadeAnimation*)tex.get_animation(12);
    audio->unload_music_stream("preview");
    music_playing = false;
}

std::vector<Difficulty> SongBox::get_diffs() {
    std::vector<Difficulty> diffs;
    for (const auto& [diff, level] : parser.metadata.course_data) {
        diffs.push_back(Difficulty(diff));
    }
    return diffs;
}

void SongBox::load_text() {
    BaseBox::load_text();
    float font_size = utf8_char_count(text_subtitle) < 30
        ? tex.skin_config["yb_subtitle"].font_size
        : tex.skin_config["yb_subtitle"].font_size - (int)(10 * tex.screen_scale);
    subtitle = make_unique<OutlinedText>(text_subtitle, font_size, ray::WHITE, ray::BLACK, true);

    font_size = tex.skin_config["song_box_name"].font_size;
    if (utf8_char_count(text_name) >= 30)
        font_size -= (int)(10 * tex.screen_scale);
    name_black = make_unique<OutlinedText>(text_name, font_size, ray::WHITE, ray::BLACK, true);
    text_loaded = true;
}

void SongBox::update(double current_time) {
    BaseBox::update(current_time);
    diff_fade_in->update(current_time);

    if (yellow_box.has_value() && (yellow_box->left_out != nullptr) && yellow_box->left_out->is_finished && fs::exists(parser.metadata.wave) && !music_playing) {
        music_playing = true;
        audio->stop_sound("bgm");
        audio->load_music_stream(parser.metadata.wave, "preview");
        audio->play_music_stream("preview", "music");
        audio->seek_music_stream("preview", parser.metadata.demostart);
    }
}

void SongBox::close_box() {
    BaseBox::close_box();
    if (music_playing) {
        audio->stop_music_stream("preview");
        audio->unload_music_stream("preview");
        audio->play_sound("bgm", "music");
        music_playing = false;
    }
}

void SongBox::enter_box() {
    yellow_box->create_anim_2();
    diff_fade_in->start();
}

void SongBox::draw_closed() {
    BaseBox::draw_closed();

    if (!text_loaded) return;
    float name_x = position + tex.skin_config["song_box_name"].x - (int)(this->name->width / 2);
    float name_y = tex.skin_config["song_box_name"].y;
    float name_h = std::min((float)this->name->height, tex.skin_config["song_box_name"].height);
    this->name->draw({.x = name_x, .y = name_y, .y2 = name_h - this->name->height, .fade=fade->attribute});

    if (parser.ex_data.new_song)
        tex.draw_texture("yellow_box", "ex_data_new_song_balloon", {.x=position, .fade=fade->attribute});

    int highest_key = -1;
    for (int i = 0; i < (int)scores.size(); ++i) {
        if (scores[i].has_value()) highest_key = std::max(highest_key, i);
    }
    if (highest_key >= 0) {
        Score score = scores[highest_key].value();
        int frame = std::min((int)Difficulty::URA, highest_key);
        if      (score.crown == Crown::DFC)   tex.draw_texture("yellow_box", "crown_dfc",   {.frame=frame, .x=position, .fade=fade->attribute});
        else if (score.crown == Crown::FC)    tex.draw_texture("yellow_box", "crown_fc",    {.frame=frame, .x=position, .fade=fade->attribute});
        else if (score.crown >= Crown::CLEAR) tex.draw_texture("yellow_box", "crown_clear", {.frame=frame, .x=position, .fade=fade->attribute});
    }
}

void SongBox::draw_diff_select(bool is_ura) {
    BaseBox::draw_diff_select(is_ura);
    tex.draw_texture("diff_select", "back",   {.fade=diff_fade_in->attribute});
    tex.draw_texture("diff_select", "option", {.fade=diff_fade_in->attribute});
    tex.draw_texture("diff_select", "neiro",  {.fade=diff_fade_in->attribute});

    float offset_x     = tex.skin_config["yb_diff_offset_diff_select"].x;
    float offset_y     = tex.skin_config["yb_diff_offset_diff_select"].y;
    float crown_offset = tex.skin_config["yb_diff_offset_crown"].x;

    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        float cx = (diff * offset_x) + crown_offset;
        tex.draw_texture("yellow_box", "s_crown_outline", {.x=cx, .y=offset_y, .fade=std::min((float)diff_fade_in->attribute, 0.25f)});
        if (scores[diff].has_value()) {
            if      (scores[diff]->crown == Crown::DFC)   tex.draw_texture("yellow_box", "s_crown_dfc",   {.x=cx, .y=offset_y, .fade=diff_fade_in->attribute});
            else if (scores[diff]->crown == Crown::FC)    tex.draw_texture("yellow_box", "s_crown_fc",    {.x=cx, .y=offset_y, .fade=diff_fade_in->attribute});
            else if (scores[diff]->crown >= Crown::CLEAR) tex.draw_texture("yellow_box", "s_crown_clear", {.x=cx, .y=offset_y, .fade=diff_fade_in->attribute});
        }
    }

    for (int i = 0; i < 4; i++) {
        if (i == (int)Difficulty::ONI && is_ura) {
            tex.draw_texture("diff_select", "diff_tower",    {.frame=4, .x=i*offset_x, });
            tex.draw_texture("diff_select", "ura_oni_plate", {});
        } else {
            tex.draw_texture("diff_select", "diff_tower", {.frame=i, .x=i*offset_x, .fade=diff_fade_in->attribute});
        }
        if (!parser.metadata.course_data.count(i))
            tex.draw_texture("diff_select", "diff_tower_shadow", {.frame=i, .x=i*offset_x, .fade=std::min((float)diff_fade_in->attribute, 0.25f)});
    }

    float star_offset_y = tex.skin_config["yb_diff_offset_crown"].y;
    for (const auto& [course_diff, course] : parser.metadata.course_data) {
        if ((course_diff == (int)Difficulty::URA && !is_ura) ||
            (course_diff == (int)Difficulty::ONI && is_ura))
            continue;
        for (int j = 0; j < course.level; j++)
            tex.draw_texture("yellow_box", "star_ura", {.x=std::min(course_diff, (int)Difficulty::ONI)*offset_x, .y=j*star_offset_y, .fade=diff_fade_in->attribute});
        if (course.is_branching && ((int)(get_current_ms() / 1000)) % 2 == 0) {
            std::string bname = (course_diff == (int)Difficulty::URA) ? "branch_indicator_ura" : "branch_indicator_diff";
            tex.draw_texture("yellow_box", bname, {.x=std::min(course_diff, (int)Difficulty::ONI)*offset_x, .fade=diff_fade_in->attribute});
        }
    }
    draw_text();
}

void SongBox::draw_text() {
    float x = position + (yellow_box->right_out->attribute*0.90 - (yellow_box->right_out->start_position*0.90)) + yellow_box->right_out_2->attribute - yellow_box->right_out_2->start_position;
    float h = std::min((float)subtitle->height, tex.skin_config["yb_subtitle"].height);
    subtitle->draw({.x = x + tex.skin_config["yb_subtitle"].x, .y=tex.skin_config["yb_subtitle"].y - h + (float)yellow_box->top_y_out->attribute - yellow_box->top_y_out->start_position, .y2 =h - subtitle->height, .fade=open_fade->attribute});
    float name_h = std::min((float)this->name->height, tex.skin_config["song_box_name"].height) - this->name->height;
    float name_x = x + tex.skin_config["song_box_name"].x - (int)(this->name->width / 2);
    float name_y = tex.skin_config["song_box_name"].y + (float)yellow_box->top_y_out->attribute - yellow_box->top_y_out->start_position;
    name_black->draw({.x=name_x, .y=name_y, .y2=name_h, .fade=open_fade->attribute});
    name->draw({.x=name_x, .y=name_y, .y2=name_h, .fade=1 - open_fade->attribute});
}

void SongBox::draw_open() {
    if (yellow_box.has_value())
        yellow_box->draw();

    float offset = tex.skin_config["yb_diff_offset"].x;

    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        tex.draw_texture("yellow_box", "s_crown_outline", {.x=diff*offset, .fade=std::min((float)open_fade->attribute, 0.25f)});
        if (scores[diff].has_value()) {
            if      (scores[diff]->crown == Crown::DFC)           tex.draw_texture("yellow_box", "s_crown_dfc",   {.x=diff*offset, .fade=open_fade->attribute});
            else if (scores[diff]->crown == Crown::FC)            tex.draw_texture("yellow_box", "s_crown_fc",    {.x=diff*offset, .fade=open_fade->attribute});
            else if (scores[diff]->crown >= Crown::CLEAR)         tex.draw_texture("yellow_box", "s_crown_clear", {.x=diff*offset, .fade=open_fade->attribute});
        }
    }

    if      (parser.ex_data.new_audio)     tex.draw_texture("yellow_box", "ex_data_new_audio",     {.fade=open_fade->attribute});
    else if (parser.ex_data.old_audio)     tex.draw_texture("yellow_box", "ex_data_old_audio",     {.fade=open_fade->attribute});
    else if (parser.ex_data.limited_time)  tex.draw_texture("yellow_box", "ex_data_limited_time",  {.fade=open_fade->attribute});
    else if (parser.ex_data.new_song)      tex.draw_texture("yellow_box", "ex_data_new_song",      {.fade=open_fade->attribute});

    if (is_favorite)
        tex.draw_texture("yellow_box", "favorite_" + std::to_string((int)global_data.player_num) + "p", {.fade=open_fade->attribute});

    for (int i = 0; i < 4; i++) {
        tex.draw_texture("yellow_box", "difficulty_bar", {.frame=i, .x=i*offset, .fade=open_fade->attribute});
        if (!parser.metadata.course_data.count(i))
            tex.draw_texture("yellow_box", "difficulty_bar_shadow", {.frame=i, .x=i*offset, .fade=std::min((float)open_fade->attribute, 0.25f)});
    }

    float offset_y = tex.skin_config["yb_diff_offset"].y;
    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        for (int j = 0; j < course.level; j++)
            tex.draw_texture("yellow_box", "star", {.x=diff*offset, .y=j*offset_y, .fade=open_fade->attribute});
        if (course.is_branching && ((int)(get_current_ms() / 1000)) % 2 == 0)
            tex.draw_texture("yellow_box", "branch_indicator", {.x=diff*offset, .fade=open_fade->attribute});
    }
    draw_text();
}
