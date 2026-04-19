#include "diff_sort.h"

DiffSortSelect::DiffSortSelect(Statistics statistics, int prev_diff, int prev_level) : prev_diff(prev_diff), prev_level(prev_level), statistics(statistics) {
    selected_box = -1;
    selected_level = 1;
    in_level_select = false;
    confirmation = false;
    confirm_index = 1;
    num_boxes = 6;
    limits = {5, 7, 8, 10, 10};

    bg_resize = (TextureResizeAnimation*)tex.get_animation(19);
    diff_fade_in = (FadeAnimation*)tex.get_animation(20);
    box_flicker = (FadeAnimation*)tex.get_animation(21);
    bounce_up_1 = (MoveAnimation*)tex.get_animation(22);
    bounce_down_1 = (MoveAnimation*)tex.get_animation(23);
    bounce_up_2 = (MoveAnimation*)tex.get_animation(24);
    bounce_down_2 = (MoveAnimation*)tex.get_animation(25);
    blue_arrow_fade = (FadeAnimation*)tex.get_animation(29);
    blue_arrow_move = (MoveAnimation*)tex.get_animation(30);

    bg_resize->start();
    diff_fade_in->start();
    box_flicker->start();

    for (const auto& [course, levels] : statistics) {
        std::array<int, 3> sums = {0, 0, 0};
        for (const auto& [level, stats] : levels) {
            sums[0] += stats.total;
            sums[1] += stats.clears;
            sums[2] += stats.full_combos;
        }
        diff_sort_sum_stat[course] = sums;
    }

    audio->play_sound("voice_diff_sort_enter", "voice");

}

void DiffSortSelect::update(double current_ms) {
    bg_resize->update(current_ms);
    diff_fade_in->update(current_ms);
    box_flicker->update(current_ms);
    bounce_up_1->update(current_ms);
    bounce_down_1->update(current_ms);
    bounce_up_2->update(current_ms);
    bounce_down_2->update(current_ms);
}

std::optional<std::pair<int, int>> DiffSortSelect::input_select() {
    if (confirmation) {
        if (confirm_index == 0) {
            confirmation = false;
        } else if (confirm_index == 1) {
            return {{selected_box, selected_level}};
        } else if (confirm_index == 2) {
            confirmation = false;
            in_level_select = false;
            return std::nullopt;
        }
    } else if (in_level_select) {
        confirmation = true;
        bounce_up_1->start();
        bounce_down_1->start();
        bounce_up_2->start();
        bounce_down_2->start();
        confirm_index = 1;
        audio->play_sound("voice_diff_sort_confirm", "voice");
        return std::nullopt;
    }
    if (selected_box == -1) return {{-1, -1}};
    if (selected_box == 5) return {{prev_diff, prev_level}};

    audio->play_sound("voice_diff_sort_level", "voice");
    in_level_select = true;
    bg_resize->start();
    diff_fade_in->start();
    selected_level = std::min(selected_level, limits[selected_box]);
    return std::nullopt;
}

void DiffSortSelect::input_left() {
    if (confirmation) {
        confirm_index = std::max(confirm_index - 1, 0);
    } else if (in_level_select) {
        selected_level = std::max(selected_level - 1, 1);
    } else {
        selected_box = std::max(selected_box - 1, -1);
    }
}

void DiffSortSelect::input_right() {
    if (confirmation) {
        confirm_index = std::min(confirm_index + 1, 2);
    } else if (in_level_select) {
        selected_level = std::min(selected_level + 1, limits[selected_box]);
    } else {
        selected_box = std::min(selected_box + 1, num_boxes - 1);
    }
}

void DiffSortSelect::draw_statistics() {
    std::string player_num_str = std::to_string((int)global_data.player_num);
    tex.draw_texture(tex_id_map.at("diff_sort/stat_bg_" + player_num_str + "p"));
    tex.draw_texture(DIFF_SORT::STAT_OVERLAY);
    tex.draw_texture(DIFF_SORT::STAT_DIFF, {.frame=std::min(selected_box, 4)});

    if (in_level_select || selected_box == 5) {
        tex.draw_texture(DIFF_SORT::STAT_STARX);
        std::string counter;
        if (selected_box == 5) {
            tex.draw_texture(DIFF_SORT::STAT_PREV);
            counter = std::to_string(prev_level);
        } else {
            counter = std::to_string(selected_level);
        }
        float margin = tex.skin_config[SC::DIFF_SORT_MARGIN_1].x;
        float total_width = counter.size() * margin;
        for (size_t i = 0; i < counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture(DIFF_SORT::STAT_NUM_STAR, {.frame=digit, .x=tex.skin_config[SC::DIFF_SORT_STAT_NUM_STAR].x-(counter.size() - i) * margin, .y=tex.skin_config[SC::DIFF_SORT_STAT_NUM_STAR].y});
        }

        counter = std::to_string(statistics[selected_box][selected_level].total);
        if (selected_box == 5) counter = std::to_string(statistics[prev_diff][prev_level].total);
        margin = tex.skin_config[SC::DIFF_SORT_MARGIN_2].x;
        total_width = counter.size() * margin;
        for (size_t i = 0; i < counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture(DIFF_SORT::STAT_NUM, {.frame=digit, .x=-(total_width/2)+(i*margin)});
        }

        for (int j = 0; j < 2; j++) {
            std::string counter;
            if (selected_box == 5) {
                counter = std::to_string(statistics[prev_diff][prev_level].total);
            } else {
                counter = std::to_string(statistics[selected_box][selected_level].total);
            }

            margin = tex.skin_config[SC::DIFF_SORT_MARGIN_3].x;
            total_width = counter.size() * margin;

            for (int i = 0; i < (int)counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture(DIFF_SORT::STAT_NUM_SMALL, {.frame=digit, .x=-(total_width / 2) + (i * margin), .index=j});
            }
        }

        for (int j = 0; j < 2; j++) {
            std::string counter;
            if (selected_box == 5) {
                if (j + 1 == 1) {
                    counter = std::to_string(statistics[prev_diff][prev_level].clears);
                } else {
                    counter = std::to_string(statistics[prev_diff][prev_level].full_combos);
                }
            } else {
                if (j + 1 == 1) {
                    counter = std::to_string(statistics[selected_box][selected_level].clears);
                } else {
                    counter = std::to_string(statistics[selected_box][selected_level].full_combos);
                }
            }

            margin = tex.skin_config[SC::DIFF_SORT_MARGIN_1].x;
            total_width = counter.size() * margin;

            for (int i = 0; i < (int)counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture(DIFF_SORT::STAT_NUM_STAR, {.frame=digit, .x=-(total_width / 2) + (i * margin), .index=j+1});
            }
        }
    } else {
        std::string counter = std::to_string(diff_sort_sum_stat[selected_box][0]);
        float margin = tex.skin_config[SC::DIFF_SORT_MARGIN_2].x;
        float total_width = counter.size() * margin;
        for (size_t i = 0; i < counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture(DIFF_SORT::STAT_NUM, {.frame=digit, .x=-(total_width/2)+(i*margin)});
        }

        for (int j = 0; j < 2; j++) {
            counter = std::to_string(diff_sort_sum_stat[selected_box][0]);
            margin = tex.skin_config[SC::DIFF_SORT_MARGIN_3].x;
            total_width = counter.size() * margin;
            for (size_t i = 0; i < counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture(DIFF_SORT::STAT_NUM_SMALL, {.frame=digit, .x=-(total_width/2)+(i*margin), .index=j});
            }
        }

        for (int j = 0; j < 2; j++) {
            if (j + 1 == 1) {
                counter = std::to_string(diff_sort_sum_stat[selected_box][1]);
            } else {
                counter = std::to_string(diff_sort_sum_stat[selected_box][2]);
            }
            margin = tex.skin_config[SC::DIFF_SORT_MARGIN_1].x;
            total_width = counter.size() * margin;
            for (size_t i = 0; i < counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture(DIFF_SORT::STAT_NUM_STAR, {.frame=digit, .x=-(total_width/2)+(i*margin), .index=j+1});
            }
        }
    }
}

void DiffSortSelect::draw_diff_select() {
    tex.draw_texture(DIFF_SORT::BACKGROUND, {.scale=(float)bg_resize->attribute, .center=true});

    tex.draw_texture(DIFF_SORT::BACK, {.fade=diff_fade_in->attribute});
    float offset = tex.skin_config[SC::DIFF_SORT_OFFSET].x;
    for (size_t i = 0; i < num_boxes; i++) {
        if (i == selected_box) {
            tex.draw_texture(DIFF_SORT::BOX_HIGHLIGHT, {.x=(offset*i), .fade=diff_fade_in->attribute});
            tex.draw_texture(DIFF_SORT::BOX_TEXT_HIGHLIGHT, {.frame=(int)i, .x=(offset*i), .fade=diff_fade_in->attribute});
        } else {
            tex.draw_texture(DIFF_SORT::BOX, {.x=(offset*i), .fade=diff_fade_in->attribute});
            tex.draw_texture(DIFF_SORT::BOX_TEXT, {.frame=(int)i, .x=(offset*i), .fade=diff_fade_in->attribute});
        }
    }

    if (selected_box == -1) {
        tex.draw_texture(DIFF_SORT::BACK_OUTLINE, {.fade=box_flicker->attribute});
    } else {
        tex.draw_texture(DIFF_SORT::BOX_OUTLINE, {.x=(offset*selected_box), .fade=box_flicker->attribute});
    }

    for (size_t i = 0; i < num_boxes; i++) {
        if (i < 5) {
            tex.draw_texture(DIFF_SORT::BOX_DIFF, {.frame=(int)i, .x=(offset*i)});
        }
    }
    if (selected_box != -1 && selected_box != num_boxes - 1) {
        draw_statistics();
    }
}

void DiffSortSelect::draw_level_select() {
    tex.draw_texture(DIFF_SORT::BACKGROUND, {.scale=(float)bg_resize->attribute, .center=true});
    if (confirmation) {
        tex.draw_texture(DIFF_SORT::STAR_SELECT_PROMPT);
    } else {
        tex.draw_texture(DIFF_SORT::STAR_SELECT_TEXT, {.fade=diff_fade_in->attribute});
    }
    tex.draw_texture(DIFF_SORT::STAR_LIMIT, {.frame=selected_box, .fade=diff_fade_in->attribute});
    tex.draw_texture(DIFF_SORT::LEVEL_BOX, {.fade=diff_fade_in->attribute});
    tex.draw_texture(DIFF_SORT::DIFF, {.frame=selected_box, .fade=diff_fade_in->attribute});
    tex.draw_texture(DIFF_SORT::STAR_NUM, {.frame=selected_level, .fade=diff_fade_in->attribute});
    for (size_t i = 0; i < selected_level; i++) {
        tex.draw_texture(DIFF_SORT::STAR, {.x=(float)(i*(40.5 * tex.screen_scale)), .fade=diff_fade_in->attribute});
    }

    if (confirmation) {
        TextureObject* texture = tex.textures[DIFF_SORT::LEVEL_BOX].get();
        ray::DrawRectangle(texture->x[0], texture->y[0], texture->x2[0], texture->y2[0], ray::Fade(ray::BLACK, 0.5));
        float y = -bounce_up_1->attribute + bounce_down_1->attribute - bounce_up_2->attribute + bounce_down_2->attribute;
        float offset = tex.skin_config[SC::DIFF_SORT_OFFSET_2].x;
        for (size_t i = 0; i < 3; i++) {
            if (i == confirm_index) {
                tex.draw_texture(DIFF_SORT::SMALL_BOX_HIGHLIGHT, {.x=(i*offset), .y=y});
                tex.draw_texture(DIFF_SORT::SMALL_BOX_TEXT_HIGHLIGHT, {.frame=(int)i, .x=(i*offset), .y=y});
                tex.draw_texture(DIFF_SORT::SMALL_BOX_OUTLINE, {.x=(i*offset), .y=y, .fade=box_flicker->attribute});
            } else {
                tex.draw_texture(DIFF_SORT::SMALL_BOX, {.x=(i*offset), .y=y});
                tex.draw_texture(DIFF_SORT::SMALL_BOX_TEXT, {.frame=(int)i, .x=(i*offset), .y=y});
            }
        }
    } else {
        tex.draw_texture(DIFF_SORT::PONGOS);
        if (selected_level != 1) {
            tex.draw_texture(DIFF_SORT::ARROW, {.x=(float)-blue_arrow_move->attribute, .fade=blue_arrow_fade->attribute, .index=0});
        }
        if (selected_level != limits[selected_box]) {
            tex.draw_texture(DIFF_SORT::ARROW, {.mirror="horizontal", .x=(float)blue_arrow_move->attribute, .fade=blue_arrow_fade->attribute, .index=1});
        }
    }
    draw_statistics();
}

void DiffSortSelect::draw() {
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Fade(ray::BLACK, 0.6));
    if (in_level_select) {
        draw_level_select();
    } else {
        draw_diff_select();
    }
}
