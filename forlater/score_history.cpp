#include "score_history.h"

ScoreHistory::ScoreHistory(const std::map<int, ScoreRow*>& scores_in, double current_ms) {
    for (const auto& [k, v] : scores_in)
        if (v != nullptr) scores[k] = v;

    for (const auto& [k, v] : scores)
        difficulty_keys.push_back(k);

    curr_difficulty_index = 0;
    curr_difficulty_index = (curr_difficulty_index + 1) % (int)difficulty_keys.size();
    curr_difficulty       = difficulty_keys[curr_difficulty_index];
    curr_score            = scores.at(curr_difficulty)->score;
    curr_score_su         = scores.at(curr_difficulty)->score;
    last_ms               = current_ms;
    is_long               = true;
}

void ScoreHistory::update(double current_ms) {
    if (current_ms >= last_ms + 1000.0) {
        last_ms               = current_ms;
        curr_difficulty_index = (curr_difficulty_index + 1) % (int)difficulty_keys.size();
        curr_difficulty       = difficulty_keys[curr_difficulty_index];
        curr_score            = scores.at(curr_difficulty)->score;
        curr_score_su         = scores.at(curr_difficulty)->score;
    }
}

void ScoreHistory::draw_long() {
    tex.draw_texture("leaderboard", "background_2", {});
    tex.draw_texture("leaderboard", "title",        {.index=(int)is_long});

    std::string score_method = global_data.config->general.score_method;
    if (score_method == ScoreMethod::SHINUCHI) {
        if (curr_difficulty == (int)Difficulty::URA)
            tex.draw_texture("leaderboard", "shinuchi_ura", {.index=(int)is_long});
        else
            tex.draw_texture("leaderboard", "shinuchi",     {.index=(int)is_long});
        tex.draw_texture("leaderboard", "pts", {.color=ray::WHITE, .index=(int)is_long});
    } else if (score_method == ScoreMethod::GEN3) {
        tex.draw_texture("leaderboard", "normal", {.index=(int)is_long});
        tex.draw_texture("leaderboard", "pts",    {.color=ray::BLACK, .index=(int)is_long});
    }

    tex.draw_texture("leaderboard", "difficulty", {.frame=curr_difficulty, .index=(int)is_long});

    float bg_offset_y = tex.skin_config["score_info_bg_offset"].y;
    for (int i = 0; i < 4; i++)
        tex.draw_texture("leaderboard", "normal", {.y=bg_offset_y + (i * bg_offset_y), .index=(int)is_long});

    tex.draw_texture("leaderboard", "judge_good",     {});
    tex.draw_texture("leaderboard", "judge_ok",       {});
    tex.draw_texture("leaderboard", "judge_bad",      {});
    tex.draw_texture("leaderboard", "judge_drumroll", {});

    ScoreRow* row = scores.at(curr_difficulty);
    std::array<int, 5> fields = { row->score, row->good, row->ok, row->bad, row->drumroll };

    float counter_margin_w = tex.skin_config["score_info_counter_margin"].width;
    float counter_margin_x = tex.skin_config["score_info_counter_margin"].x;

    for (int j = 0; j < (int)fields.size(); j++) {
        if (j == (int)Difficulty::TOWER) continue;

        std::string counter = std::to_string(fields[j]);
        for (int i = 0; i < (int)counter.size(); i++) {
            int digit = counter[i] - '0';
            if (j == 0) {
                float cx = -((counter.size() * counter_margin_w) / 2.0f) + (i * counter_margin_w);
                if (score_method == ScoreMethod::SHINUCHI)
                    tex.draw_texture("leaderboard", "counter", {.color=ray::WHITE, .frame=digit, .x=cx, .index=(int)is_long});
                else
                    tex.draw_texture("leaderboard", "counter", {.color=ray::BLACK, .frame=digit, .x=cx, .index=(int)is_long});
            } else {
                float cx = -(float)(counter.size() - i) * counter_margin_x;
                tex.draw_texture("leaderboard", "judge_num", {.frame=digit, .x=cx, .y=(float)j * bg_offset_y});
            }
        }
    }
}

void ScoreHistory::draw() {
    if (is_long) {
        draw_long();
        return;
    }

    tex.draw_texture("leaderboard", "background", {});
    tex.draw_texture("leaderboard", "title",      {});

    if (curr_difficulty == (int)Difficulty::URA) {
        tex.draw_texture("leaderboard", "normal_ura",   {});
        tex.draw_texture("leaderboard", "shinuchi_ura", {});
    } else {
        tex.draw_texture("leaderboard", "normal",   {});
        tex.draw_texture("leaderboard", "shinuchi", {});
    }

    ray::Color color = ray::BLACK;
    if (curr_difficulty == (int)Difficulty::URA) {
        color = ray::WHITE;
        tex.draw_texture("leaderboard", "ura", {});
    }

    float bg_offset_y      = tex.skin_config["score_info_bg_offset"].y;
    float counter_margin_w = tex.skin_config["score_info_counter_margin"].width;

    tex.draw_texture("leaderboard", "pts", {.color=color});
    tex.draw_texture("leaderboard", "pts", {.color=color, .y=bg_offset_y});
    tex.draw_texture("leaderboard", "difficulty", {.frame=curr_difficulty});

    // curr_score row
    {
        std::string counter = std::to_string(curr_score);
        float total_width = counter.size() * counter_margin_w;
        for (int i = 0; i < (int)counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture("leaderboard", "counter", {
                .color=color,
                .frame=digit,
                .x=-(total_width / 2.0f) + (i * counter_margin_w)
            });
        }
    }

    // curr_score_su row
    {
        std::string counter = std::to_string(curr_score_su);
        float total_width = counter.size() * counter_margin_w;
        for (int i = 0; i < (int)counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture("leaderboard", "counter", {
                .color=ray::WHITE,
                .frame=digit,
                .x=-(total_width / 2.0f) + (i * counter_margin_w),
                .y=bg_offset_y,
            });
        }
    }
}
