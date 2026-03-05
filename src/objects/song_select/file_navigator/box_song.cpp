#include "box_song.h"

SongBox::SongBox(const fs::path& path,
                 const std::optional<ray::Color>& back_color,
                 const std::optional<ray::Color>& fore_color,
                 TextureIndex texture_index,
                 TJAParser parser)
    : BaseBox(path, back_color, fore_color, texture_index)
{
    this->parser = parser;
    parser.get_metadata();
    auto& titles = parser.metadata.title;
    const std::string& lang = global_data.config->general.language;
    text_name = titles.count(lang) ? titles.at(lang) : titles.at("en");

    auto& subtitles = parser.metadata.subtitle;
    text_subtitle = subtitles.count(lang) ? subtitles.at(lang) : subtitles.at("en");

    is_favorite = false;
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

/*void SongBox::get_scores() {
    if (parser->metadata.course_data.empty()) return;

    std::vector<std::string> hash_values;
    for (const auto& [diff, course] : parser->metadata.course_data) {
        if (hash.count(diff))
            hash_values.push_back(hash.at(diff));
    }
    if (hash_values.empty()) return;

    sqlite3* db;
    if (sqlite3_open(global_data.score_db.c_str(), &db) != SQLITE_OK) return;

    std::string placeholders;
    for (size_t i = 0; i < hash_values.size(); i++) {
        if (i > 0) placeholders += ",";
        placeholders += "?";
    }

    std::string query =
        "SELECT hash, score, good, ok, bad, drumroll, clear "
        "FROM Scores WHERE hash IN (" + placeholders + ")";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }

    for (int i = 0; i < (int)hash_values.size(); i++)
        sqlite3_bind_text(stmt, i + 1, hash_values[i].c_str(), -1, SQLITE_STATIC);

    std::map<std::string, ScoreRow*> hash_to_score;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string row_hash = (const char*)sqlite3_column_text(stmt, 0);
        auto* row = new ScoreRow();
        row->score    = sqlite3_column_int(stmt, 1);
        row->good     = sqlite3_column_int(stmt, 2);
        row->ok       = sqlite3_column_int(stmt, 3);
        row->bad      = sqlite3_column_int(stmt, 4);
        row->drumroll = sqlite3_column_int(stmt, 5);
        row->crown    = static_cast<Crown>(sqlite3_column_int(stmt, 6));
        hash_to_score[row_hash] = row;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    for (auto& [diff, row] : scores) delete row;
    scores.clear();

    for (const auto& [diff, course] : parser->metadata.course_data) {
        if (!hash.count(diff)) continue;
        const std::string& diff_hash = hash.at(diff);
        scores[diff] = hash_to_score.count(diff_hash) ? hash_to_score.at(diff_hash) : nullptr;
    }
    score_history = nullptr;
    }*/

void SongBox::update(double current_time) {
    BaseBox::update(current_time);
}

void SongBox::draw_closed(float outer_fade_override) {
    BaseBox::draw_closed(outer_fade_override);

    if (!text_loaded) return;
    float name_x = position + tex.skin_config["song_box_name"].x - (int)(this->name->width / 2);
    float name_y = tex.skin_config["song_box_name"].y;
    float name_h = std::min((float)this->name->height, tex.skin_config["song_box_name"].height);
    this->name->draw({.x = name_x, .y = name_y, .y2 = name_h - this->name->height, .fade = outer_fade_override});

    if (parser.ex_data.new_song)
        tex.draw_texture("yellow_box", "ex_data_new_song_balloon", {.x=position, .fade=outer_fade_override});

    /*int highest_key = -1;
    for (const auto& [diff, row] : scores) {
        if (row != nullptr) highest_key = std::max(highest_key, diff);
    }

    if (highest_key >= 0) {
        ScoreRow* score = scores.at(highest_key);
        int frame = std::min((int)Difficulty::URA, highest_key);
        if      (score->crown == Crown::DFC)           tex.draw_texture("yellow_box", "crown_dfc",   {.frame=frame, .x=x, .y=y, .fade=outer_fade_override});
        else if (score->crown == Crown::FC)            tex.draw_texture("yellow_box", "crown_fc",    {.frame=frame, .x=x, .y=y, .fade=outer_fade_override});
        else if (score->crown >= Crown::CLEAR)         tex.draw_texture("yellow_box", "crown_clear", {.frame=frame, .x=x, .y=y, .fade=outer_fade_override});
        }*/
}

void SongBox::draw_diff_select(std::optional<float> fade_override) {
    BaseBox::draw_diff_select(fade_override);
    tex.draw_texture("diff_select", "back",   {});
    tex.draw_texture("diff_select", "option", {});
    tex.draw_texture("diff_select", "neiro",  {});

    float offset_x     = tex.skin_config["yb_diff_offset_diff_select"].x;
    float offset_y     = tex.skin_config["yb_diff_offset_diff_select"].y;
    float crown_offset = tex.skin_config["yb_diff_offset_crown"].x;

    /*for (const auto& [diff, course] : tja->metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        float cx = (diff * offset_x) + crown_offset;
        if (scores.count(diff) && scores[diff] != nullptr) {
            if      (scores[diff]->crown == Crown::DFC)   tex.draw_texture("yellow_box", "s_crown_dfc",   {.x=cx, .y=offset_y, });
            else if (scores[diff]->crown == Crown::FC)    tex.draw_texture("yellow_box", "s_crown_fc",    {.x=cx, .y=offset_y, });
            else if (scores[diff]->crown >= Crown::CLEAR) tex.draw_texture("yellow_box", "s_crown_clear", {.x=cx, .y=offset_y, });
        }
        tex.draw_texture("yellow_box", "s_crown_outline", {.x=cx, .y=offset_y, .fade=std::min((float)fade_in->attribute, 0.25f)});
    }*/

    for (int i = 0; i < 4; i++) {
        /*if (i == (int)Difficulty::ONI) {
            tex.draw_texture("diff_select", "diff_tower",    {.frame=4, .x=i*offset_x, });
            tex.draw_texture("diff_select", "ura_oni_plate", {});
        } else {*/
            tex.draw_texture("diff_select", "diff_tower", {.frame=i, .x=i*offset_x, });
            //}
        if (!parser.metadata.course_data.count(i))
            tex.draw_texture("diff_select", "diff_tower_shadow", {.frame=i, .x=i*offset_x, .fade=0.25f});
    }

    float star_offset_y = tex.skin_config["yb_diff_offset_crown"].y;
    for (const auto& [course_diff, course] : parser.metadata.course_data) {
        /*if ((course_diff == (int)Difficulty::URA && !is_ura) ||
            (course_diff == (int)Difficulty::ONI && is_ura))
            continue;*/
        for (int j = 0; j < course.level; j++)
            tex.draw_texture("yellow_box", "star_ura", {.x=std::min(course_diff, (int)Difficulty::ONI)*offset_x, .y=j*star_offset_y, });
        if (course.is_branching && ((int)(get_current_ms() / 1000)) % 2 == 0) {
            std::string bname = (course_diff == (int)Difficulty::URA) ? "branch_indicator_ura" : "branch_indicator_diff";
            tex.draw_texture("yellow_box", bname, {.x=std::min(course_diff, (int)Difficulty::ONI)*offset_x, });
        }
    }
    draw_text();
}

void SongBox::draw_text() {
    float x = position + (yellow_box->right_out->attribute*0.85 - (yellow_box->right_out->start_position*0.85)) + yellow_box->right_out_2->attribute - yellow_box->right_out_2->start_position;
    float h = std::min((float)subtitle->height, tex.skin_config["yb_subtitle"].height);
    subtitle->draw({.x = x + tex.skin_config["yb_subtitle"].x, .y=tex.skin_config["yb_subtitle"].y, .y2 = h - subtitle->height});
    float name_h = std::min((float)this->name->height, tex.skin_config["song_box_name"].height) - this->name->height;
    name_black->draw({.x=x + tex.skin_config["yb_name"].x, .y=tex.skin_config["yb_name"].y + (float)yellow_box->top_y_out->attribute, .y2=name_h});
}

void SongBox::draw_open(std::optional<float> fade_override) {
    if (yellow_box.has_value())
        yellow_box->draw();

    float offset = tex.skin_config["yb_diff_offset"].x;

    /*for (const auto& [diff, course] : parser->metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        if (scores.count(diff) && scores[diff] != nullptr) {
            if      (scores[diff]->crown == Crown::DFC)           tex.draw_texture("yellow_box", "s_crown_dfc",   {.color=color, .x=diff*offset});
            else if (scores[diff]->crown == Crown::FC)            tex.draw_texture("yellow_box", "s_crown_fc",    {.color=color, .x=diff*offset});
            else if (scores[diff]->crown >= Crown::CLEAR)         tex.draw_texture("yellow_box", "s_crown_clear", {.color=color, .x=diff*offset});
        }
        tex.draw_texture("yellow_box", "s_crown_outline", {.x=diff*offset, .fade=std::min(fade_val, 0.25f)});
    }*/

    if      (parser.ex_data.new_audio)     tex.draw_texture("yellow_box", "ex_data_new_audio",     {});
    else if (parser.ex_data.old_audio)     tex.draw_texture("yellow_box", "ex_data_old_audio",     {});
    else if (parser.ex_data.limited_time)  tex.draw_texture("yellow_box", "ex_data_limited_time",  {});
    else if (parser.ex_data.new_song)      tex.draw_texture("yellow_box", "ex_data_new_song",      {});

    if (is_favorite)
        tex.draw_texture("yellow_box", "favorite_" + std::to_string((int)global_data.player_num) + "p", {});

    for (int i = 0; i < 4; i++) {
        tex.draw_texture("yellow_box", "difficulty_bar", {.frame=i, .x=i*offset});
        if (!parser.metadata.course_data.count(i))
            tex.draw_texture("yellow_box", "difficulty_bar_shadow", {.frame=i, .x=i*offset, .fade=0.25f});
    }

    float offset_y = tex.skin_config["yb_diff_offset"].y;
    for (const auto& [diff, course] : parser.metadata.course_data) {
        if (Difficulty(diff) >= Difficulty::URA) continue;
        for (int j = 0; j < course.level; j++)
            tex.draw_texture("yellow_box", "star", {.x=diff*offset, .y=j*offset_y});
        if (course.is_branching && ((int)(get_current_ms() / 1000)) % 2 == 0)
            tex.draw_texture("yellow_box", "branch_indicator", {.x=diff*offset});
    }
    draw_text();
}
