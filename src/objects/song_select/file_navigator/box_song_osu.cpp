#include "box_song_osu.h"

SongBoxOsu::SongBoxOsu(const fs::path& path, const BoxDef& box_def, SongParser parser)
    : SongBox(path, box_def, parser)
{
    this->parser = parser;
    parser.get_metadata();
    text_name = parser.get_difficulty_name();

    const std::string& lang = global_data.config->general.language;
    auto& subtitles = parser.metadata.subtitle;
    text_subtitle = subtitles.count(lang) ? subtitles.at(lang) : subtitles.at("en");

    is_favorite = false;
    diff_fade_in = (FadeAnimation*)tex.get_animation(12);
    refresh_scores();
}

void SongBoxOsu::draw_closed() {
    BaseBox::draw_closed();

    if (!text_loaded) return;
    float name_x = position + tex.skin_config[SC::SONG_BOX_NAME].x - (int)(this->name->width / 2);
    float name_y = tex.skin_config[SC::SONG_BOX_NAME].y;
    float name_h = std::min((float)this->name->height, tex.skin_config[SC::SONG_BOX_NAME].height);
    this->name->draw({.x = name_x, .y = name_y, .y2 = name_h - this->name->height, .fade=fade->attribute});

    int highest_key = -1;
    for (int i = 0; i < (int)scores.size(); ++i) {
        if (scores[i].has_value() && parser.metadata.course_data.count(i)) highest_key = std::max(highest_key, i);
    }
    if (highest_key >= 0) {
        Score score = scores[highest_key].value();
        int frame = std::min((int)Difficulty::URA, highest_key);
        if      (score.crown == Crown::DFC)   tex.draw_texture(YELLOW_BOX::CROWN_DFC,   {.frame=frame, .x=position, .fade=fade->attribute});
        else if (score.crown == Crown::FC)    tex.draw_texture(YELLOW_BOX::CROWN_FC,    {.frame=frame, .x=position, .fade=fade->attribute});
        else if (score.crown >= Crown::CLEAR) tex.draw_texture(YELLOW_BOX::CROWN_CLEAR, {.frame=frame, .x=position, .fade=fade->attribute});
    }
}

void SongBoxOsu::draw_open() {
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_LEFT,  {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM,       {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_RIGHT, {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_RIGHT,        {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_TOP_RIGHT,    {.x=position, .fade=open_fade->attribute, .index=1});
    if (yellow_box.has_value())
        yellow_box->draw();

    float offset = tex.skin_config[SC::YB_DIFF_OFFSET].x;

    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        tex.draw_texture(YELLOW_BOX::S_CROWN_OUTLINE, {.x=diff*offset, .fade=std::min((float)open_fade->attribute, 0.25f)});
        if (scores[diff].has_value()) {
            if      (scores[diff]->crown == Crown::DFC)   tex.draw_texture(YELLOW_BOX::S_CROWN_DFC,   {.x=diff*offset, .fade=open_fade->attribute});
            else if (scores[diff]->crown == Crown::FC)    tex.draw_texture(YELLOW_BOX::S_CROWN_FC,    {.x=diff*offset, .fade=open_fade->attribute});
            else if (scores[diff]->crown >= Crown::CLEAR) tex.draw_texture(YELLOW_BOX::S_CROWN_CLEAR, {.x=diff*offset, .fade=open_fade->attribute});
        }
    }

    if (global_data.config->general.display_bpm)
        bpm_text->draw({.x = tex.skin_config[SC::SONG_BOX_BPM].x, .y = tex.skin_config[SC::SONG_BOX_BPM].y, .fade=open_fade->attribute});

    if (is_favorite)
        tex.draw_texture(tex_id_map.at("yellow_box/favorite_" + std::to_string((int)global_data.player_num) + "p"), {.fade=open_fade->attribute});

    for (int i = 0; i < 4; i++) {
        tex.draw_texture(YELLOW_BOX::DIFFICULTY_BAR,        {.frame=i, .x=i*offset, .fade=open_fade->attribute});
        if (!parser.metadata.course_data.count(i))
            tex.draw_texture(YELLOW_BOX::DIFFICULTY_BAR_SHADOW, {.frame=i, .x=i*offset, .fade=std::min((float)open_fade->attribute, 0.25f)});
    }

    float offset_y = tex.skin_config[SC::YB_DIFF_OFFSET].y;
    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        for (int j = 0; j < course.level; j++)
            tex.draw_texture(YELLOW_BOX::STAR, {.x=diff*offset, .y=j*offset_y, .fade=open_fade->attribute});
        if (course.is_branching && ((int)(get_current_ms() / 1000)) % 2 == 0)
            tex.draw_texture(YELLOW_BOX::BRANCH_INDICATOR, {.x=diff*offset, .fade=open_fade->attribute});
    }
    draw_text();
}
