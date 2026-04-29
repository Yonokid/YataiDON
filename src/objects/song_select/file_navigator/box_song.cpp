#include "box_song.h"
#include "raylib.h"

SongBox::SongBox(const fs::path& path, const BoxDef& box_def, SongParser parser)
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
    refresh_scores();
}

void SongBox::refresh_scores() {
    hashes = scores_manager.get_hashes(path);
    for (int i = 0; i < 5; i++) {
        scores[i] = scores_manager.get_score(hashes[i], i, 1);
    }
    score_history.reset();
}

void SongBox::reset() {
    BaseBox::reset();
    diff_fade_in = (FadeAnimation*)tex.get_animation(12);
    if (audio->is_music_stream_valid("preview")) {
        audio->unload_music_stream("preview");
    }
    music_playing = false;
    score_history.reset();
    box_opened_at = 0.0;
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
        ? tex.skin_config[SC::YB_SUBTITLE].font_size
        : tex.skin_config[SC::YB_SUBTITLE].font_size - (int)(10 * tex.screen_scale);
    subtitle = make_unique<OutlinedText>(text_subtitle, font_size, ray::WHITE, ray::BLACK, true);

    font_size = tex.skin_config[SC::SONG_BOX_NAME].font_size;
    if (utf8_char_count(text_name) >= 30)
        font_size -= (int)(10 * tex.screen_scale);
    name_black = make_unique<OutlinedText>(text_name, font_size, ray::WHITE, ray::BLACK, true);
    bpm_text = make_unique<OutlinedText>("BPM\n" + std::to_string(static_cast<int>(parser.metadata.bpm)), tex.skin_config[SC::SONG_BOX_BPM].font_size, ray::WHITE, ray::BLACK, false);
    if (exists(parser.metadata.preimage)) {
        preimage = ray::LoadTexture(parser.metadata.preimage.string().c_str());
        ray::GenTextureMipmaps(&preimage.value());
        ray::SetTextureFilter(preimage.value(), ray::TEXTURE_FILTER_TRILINEAR);
    }
    text_loaded = true;
}

void SongBox::update(double current_time) {
    BaseBox::update(current_time);
    diff_fade_in->update(current_time);

    if (yellow_box.has_value() && (yellow_box->left_out != nullptr) && yellow_box->left_out->is_finished && fs::exists(parser.metadata.wave) && !music_playing) {
        music_playing = true;
        audio->stop_sound("bgm");
        audio->load_music_stream(parser.metadata.wave, "preview");
        if (audio->is_music_stream_valid("preview")) {
            audio->play_music_stream("preview", "music");
            audio->seek_music_stream("preview", parser.metadata.demostart);
        }
    }

    if (!score_history) {
        for (const auto& s : scores) {
            if (s.has_value()) {
                score_history = std::make_unique<ScoreHistory>(scores, current_time);
                break;
            }
        }
    }

    if (score_history)
        score_history->update(current_time);
}

void SongBox::expand_box() {
    BaseBox::expand_box();
    box_opened_at = get_current_ms();
}

void SongBox::close_box() {
    BaseBox::close_box();
    box_opened_at = 0.0;
    if (music_playing) {
        if (audio->is_music_stream_valid("preview")) {
            audio->stop_music_stream("preview");
            audio->unload_music_stream("preview");
        }
        audio->play_sound("bgm", "music");
        music_playing = false;
    }
}

void SongBox::draw_score_history() {
    if (!score_history) return;
    if (!yellow_box_opened) return;
    if (get_current_ms() < box_opened_at + 3000.0) return;
    score_history->draw();
}

void SongBox::enter_box() {
    yellow_box->create_anim_2();
    diff_fade_in->start();
}

void SongBox::draw_closed() {
    BaseBox::draw_closed();

    if (!text_loaded) return;
    float name_x = position + tex.skin_config[SC::SONG_BOX_NAME].x - (int)(this->name->width / 2);
    float name_y = tex.skin_config[SC::SONG_BOX_NAME].y;
    float name_h = std::min((float)this->name->height, tex.skin_config[SC::SONG_BOX_NAME].height);
    this->name->draw({.x = name_x, .y = name_y, .y2 = name_h - this->name->height, .fade=fade->attribute});

    if (preimage.has_value()) {
        tex.draw_texture(YELLOW_BOX::PREIMAGE_BG, {.x=position, .fade=fade->attribute});
        ray::Rectangle src = {0, 0, (float)preimage->width, (float)preimage->height};
        ray::Rectangle dest = {position + tex.skin_config[SC::PREIMAGE].x, tex.skin_config[SC::PREIMAGE].y, tex.skin_config[SC::PREIMAGE].width, tex.skin_config[SC::PREIMAGE].height};
        ray::DrawTexturePro(preimage.value(), src, dest, {0,0}, 0, ray::Fade(ray::WHITE, fade->attribute));
    } else if (parser.ex_data.limited_time)
        tex.draw_texture(YELLOW_BOX::EX_DATA_LIMITED_TIME_BALLOON, {.x=position, .fade=fade->attribute});
    else if (is_new)
        tex.draw_texture(YELLOW_BOX::EX_DATA_NEW_SONG_BALLOON, {.x=position, .fade=fade->attribute});

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

void SongBox::draw_diff_select(bool is_ura) {
    BaseBox::draw_diff_select(is_ura);
    tex.draw_texture(DIFF_SELECT::BACK,   {.fade=diff_fade_in->attribute});
    tex.draw_texture(DIFF_SELECT::OPTION, {.fade=diff_fade_in->attribute});
    tex.draw_texture(DIFF_SELECT::NEIRO,  {.fade=diff_fade_in->attribute});

    float offset_x     = tex.skin_config[SC::YB_DIFF_OFFSET_DIFF_SELECT].x;
    float offset_y     = tex.skin_config[SC::YB_DIFF_OFFSET_DIFF_SELECT].y;
    float crown_offset = tex.skin_config[SC::YB_DIFF_OFFSET_CROWN].x;

    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        float cx = (diff * offset_x) + crown_offset;
        tex.draw_texture(YELLOW_BOX::S_CROWN_OUTLINE, {.x=cx, .y=offset_y, .fade=std::min((float)diff_fade_in->attribute, 0.25f)});
        if (scores[diff].has_value()) {
            if      (scores[diff]->crown == Crown::DFC)   tex.draw_texture(YELLOW_BOX::S_CROWN_DFC,   {.x=cx, .y=offset_y, .fade=diff_fade_in->attribute});
            else if (scores[diff]->crown == Crown::FC)    tex.draw_texture(YELLOW_BOX::S_CROWN_FC,    {.x=cx, .y=offset_y, .fade=diff_fade_in->attribute});
            else if (scores[diff]->crown >= Crown::CLEAR) tex.draw_texture(YELLOW_BOX::S_CROWN_CLEAR, {.x=cx, .y=offset_y, .fade=diff_fade_in->attribute});
        }
    }

    for (int i = 0; i < 4; i++) {
        if (i == (int)Difficulty::ONI && is_ura) {
            tex.draw_texture(DIFF_SELECT::DIFF_TOWER,    {.frame=4, .x=i*offset_x, });
            tex.draw_texture(DIFF_SELECT::URA_ONI_PLATE, {});
        } else {
            tex.draw_texture(DIFF_SELECT::DIFF_TOWER, {.frame=i, .x=i*offset_x, .fade=diff_fade_in->attribute});
        }
        if (!parser.metadata.course_data.count(i))
            tex.draw_texture(DIFF_SELECT::DIFF_TOWER_SHADOW, {.frame=i, .x=i*offset_x, .fade=std::min((float)diff_fade_in->attribute, 0.25f)});
    }

    float star_offset_y = tex.skin_config[SC::YB_DIFF_OFFSET_CROWN].y;
    for (const auto& [course_diff, course] : parser.metadata.course_data) {
        if ((course_diff == (int)Difficulty::URA && !is_ura) ||
            (course_diff == (int)Difficulty::ONI && is_ura))
            continue;
        for (int j = 0; j < course.level; j++)
            tex.draw_texture(YELLOW_BOX::STAR_URA, {.x=std::min(course_diff, (int)Difficulty::ONI)*offset_x, .y=j*star_offset_y, .fade=diff_fade_in->attribute});
        if (course.is_branching && ((int)(get_current_ms() / 1000)) % 2 == 0) {
            std::string bname = (course_diff == (int)Difficulty::URA) ? "branch_indicator_ura" : "branch_indicator_diff";
            tex.draw_texture(tex_id_map.at("yellow_box/" + (bname)), {.x=std::min(course_diff, (int)Difficulty::ONI)*offset_x, .fade=diff_fade_in->attribute});
        }
    }
    draw_text();
}

void SongBox::draw_text() {
    float x = position + (yellow_box->right_out->attribute*0.90 - (yellow_box->right_out->start_position*0.90)) + yellow_box->right_out_2->attribute - yellow_box->right_out_2->start_position;
    float h = std::min((float)subtitle->height, tex.skin_config[SC::YB_SUBTITLE].height);
    subtitle->draw({.x = x + tex.skin_config[SC::YB_SUBTITLE].x, .y=tex.skin_config[SC::YB_SUBTITLE].y - h + (float)yellow_box->top_y_out->attribute - yellow_box->top_y_out->start_position, .y2 =h - subtitle->height, .fade=open_fade->attribute});
    float name_h = std::min((float)this->name->height, tex.skin_config[SC::SONG_BOX_NAME].height) - this->name->height;
    float name_x = x + tex.skin_config[SC::SONG_BOX_NAME].x - (int)(this->name->width / 2);
    float name_y = tex.skin_config[SC::SONG_BOX_NAME].y + (float)yellow_box->top_y_out->attribute - yellow_box->top_y_out->start_position;
    name_black->draw({.x=name_x, .y=name_y, .y2=name_h, .fade=open_fade->attribute});
    name->draw({.x=name_x, .y=name_y, .y2=name_h, .fade=1 - open_fade->attribute});
}

void SongBox::draw_open() {
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_LEFT, {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM, {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_RIGHT, {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_RIGHT, {.x=position, .fade=open_fade->attribute, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_TOP_RIGHT, {.x=position, .fade=open_fade->attribute, .index=1});
    if (yellow_box.has_value())
        yellow_box->draw();

    float offset = tex.skin_config[SC::YB_DIFF_OFFSET].x;

    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        tex.draw_texture(YELLOW_BOX::S_CROWN_OUTLINE, {.x=diff*offset, .fade=std::min((float)open_fade->attribute, 0.25f)});
        if (scores[diff].has_value()) {
            if      (scores[diff]->crown == Crown::DFC)           tex.draw_texture(YELLOW_BOX::S_CROWN_DFC,   {.x=diff*offset, .fade=open_fade->attribute});
            else if (scores[diff]->crown == Crown::FC)            tex.draw_texture(YELLOW_BOX::S_CROWN_FC,    {.x=diff*offset, .fade=open_fade->attribute});
            else if (scores[diff]->crown >= Crown::CLEAR)         tex.draw_texture(YELLOW_BOX::S_CROWN_CLEAR, {.x=diff*offset, .fade=open_fade->attribute});
        }
    }

    if      (parser.ex_data.new_audio)     tex.draw_texture(YELLOW_BOX::EX_DATA_NEW_AUDIO,     {.fade=open_fade->attribute});
    else if (parser.ex_data.old_audio)     tex.draw_texture(YELLOW_BOX::EX_DATA_OLD_AUDIO,     {.fade=open_fade->attribute});
    else if (parser.ex_data.limited_time)  tex.draw_texture(YELLOW_BOX::EX_DATA_LIMITED_TIME,  {.fade=open_fade->attribute});
    else if (is_new)      tex.draw_texture(YELLOW_BOX::EX_DATA_NEW_SONG,      {.fade=open_fade->attribute});
    if (global_data.config->general.display_bpm) {
        bpm_text->draw({.x = tex.skin_config[SC::SONG_BOX_BPM].x, .y = tex.skin_config[SC::SONG_BOX_BPM].y, .fade=open_fade->attribute});
    }

    if (is_favorite)
        tex.draw_texture(tex_id_map.at("yellow_box/favorite_" + std::to_string((int)global_data.player_num) + "p"), {.fade=open_fade->attribute});

    for (int i = 0; i < 4; i++) {
        tex.draw_texture(YELLOW_BOX::DIFFICULTY_BAR, {.frame=i, .x=i*offset, .fade=open_fade->attribute});
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
