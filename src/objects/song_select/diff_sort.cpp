#include "diff_sort.h"

DiffSortSelect::DiffSortSelect(Statistics statistics, int prev_diff, int prev_level) : prev_diff(prev_diff), prev_level(prev_level), statistics(statistics) {
    selected_box = -1;
    selected_level = 1;
    in_level_select = false;
    confirmation = false;
    confirm_index = 1;
    num_boxes = 6;
    limits = {5, 7, 8, 10};

    bg_resize = (TextureChangeAnimation*)tex.get_animation(19);
    diff_fade_in = (FadeAnimation*)tex.get_animation(20);
    box_flicker = (TextureChangeAnimation*)tex.get_animation(21);
    bounce_up_1 = (MoveAnimation*)tex.get_animation(22);
    bounce_down_1 = (MoveAnimation*)tex.get_animation(23);
    bounce_up_2 = (MoveAnimation*)tex.get_animation(24);
    bounce_down_2 = (MoveAnimation*)tex.get_animation(25);
    blue_arrow_fade = (FadeAnimation*)tex.get_animation(29);
    blue_arrow_move = (MoveAnimation*)tex.get_animation(30);

    bg_resize->start();
    diff_fade_in->start();

    std::map<int, std::array<int, 3>> diff_sort_sum_stat;

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
    if (selected_box == 5) return {{0, -1}};

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
        confirm_index = std::max(confirm_index + 1, 2);
    } else if (in_level_select) {
        selected_level = std::max(selected_level + 1, limits[selected_box]);
    } else {
        selected_box = std::max(selected_box + 1, num_boxes - 1);
    }
}

void DiffSortSelect::draw_statistics() {
    std::string player_num_str = std::to_string((int)global_data.player_num);
    tex.draw_texture("diff_sort", player_num_str + "p");
    tex.draw_texture("diff_sort", "stat_overlay");
    tex.draw_texture("diff_sort", "stat_diff", {.frame=std::min(selected_box, 3)});

    if (in_level_select || selected_box == 5) {
        tex.draw_texture("diff_sort", "stat_starx");
        std::string counter;
        if (selected_box == 5) {
            tex.draw_texture("diff_sort", "stat_prev");
            counter = std::to_string(prev_level);
        } else {
            counter = std::to_string(selected_level);
        }
        float margin = tex.skin_config["diff_sort_margin_1"].x;
        float total_width = counter.size() * margin;
        for (size_t i = 0; i < counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture("diff_sort", "stat_num_star", {.frame=digit, .x=tex.skin_config["diff_sort_stat_num_star"].y-(counter.size() - i) * margin, .y=tex.skin_config["diff_sort_stat_num_star"].y});
        }

        counter = std::to_string(statistics[selected_box][selected_level].total);
        if (selected_box == 5) counter = std::to_string(statistics[prev_diff][prev_level].total);
        margin = tex.skin_config["diff_sort_margin_2"].x;
        total_width = counter.size() * margin;
        for (size_t i = 0; i < counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture("diff_sort", "stat_num", {.frame=digit, .x=-(total_width/2)+(i*margin)});
        }

        for (int j = 0; j < 2; j++) {
            std::string counter;
            if (selected_box == 5) {
                counter = std::to_string(statistics[prev_diff][prev_level].total);
            } else {
                counter = std::to_string(statistics[selected_box][selected_level].total);
            }

            margin = tex.skin_config["diff_sort_margin_3"].x;
            total_width = counter.size() * margin;

            for (int i = 0; i < (int)counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture("diff_sort", "stat_num_small", {.frame=digit, .x=-(total_width / 2) + (i * margin), .index=j});
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

            margin = tex.skin_config["diff_sort_margin_1"].x;
            total_width = counter.size() * margin;

            for (int i = 0; i < (int)counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture("diff_sort", "stat_num_star", {.frame=digit, .x=-(total_width / 2) + (i * margin), .index=j});
            }
        }
    } else {
        std::string counter = std::to_string(diff_sort_sum_stat[selected_box][0]);
        float margin = tex.skin_config["diff_sort_margin_2"].x;
        float total_width = counter.size() * margin;
        for (size_t i = 0; i < counter.size(); i++) {
            int digit = counter[i] - '0';
            tex.draw_texture("diff_sort", "stat_num", {.frame=digit, .x=-(total_width/2)+(i*margin)});
        }

        for (int j = 0; j < 2; j++) {
            counter = std::to_string(diff_sort_sum_stat[selected_box][0]);
            margin = tex.skin_config["diff_sort_margin_2"].x;
            total_width = counter.size() * margin;
            for (size_t i = 0; i < counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture("diff_sort", "stat_num_small", {.frame=digit, .x=-(total_width/2)+(i*margin)});
            }
        }

        for (int j = 0; j < 2; j++) {
            if (j + 1 == 1) {
                counter = std::to_string(statistics[selected_box][selected_level].clears);
            } else {
                counter = std::to_string(statistics[selected_box][selected_level].full_combos);
            }
            margin = tex.skin_config["diff_sort_margin_1"].x;
            total_width = counter.size() * margin;
            for (size_t i = 0; i < counter.size(); i++) {
                int digit = counter[i] - '0';
                tex.draw_texture("diff_sort", "stat_num_star", {.frame=digit, .x=-(total_width/2)+(i*margin)});
            }
        }
    }
}

void DiffSortSelect::draw_diff_select() {
    tex.draw_texture("diff_sort", "background", {.scale=(float)bg_resize->attribute, .center=true});

    tex.draw_texture("diff_sort", "back", {.fade=diff_fade_in->attribute});
    float offset = tex.skin_config["diff_sort_offset"].x;
    for (size_t i = 0; i < num_boxes; i++) {
        if (i == selected_box) {
            tex.draw_texture("diff_sort", "box_highlight", {.x=(offset*i), .fade=diff_fade_in->attribute});
            tex.draw_texture("diff_sort", "box_text_highlight", {.frame=(int)i, .x=(offset*i), .fade=diff_fade_in->attribute});
        } else {
            tex.draw_texture("diff_sort", "box", {.x=(offset*i), .fade=diff_fade_in->attribute});
            tex.draw_texture("diff_sort", "box_text", {.frame=(int)i, .x=(offset*i), .fade=diff_fade_in->attribute});
        }
    }

    if (selected_box == -1) {
        tex.draw_texture("diff_sort", "back_outline", {.fade=box_flicker->attribute});
    } else {
        tex.draw_texture("diff_sort", "box_outline", {.x=(offset*selected_box), .fade=box_flicker->attribute});
    }

    for (size_t i = 0; i < num_boxes; i++) {
        if (i < 4) {
            tex.draw_texture("diff_sort", "box_diff", {.frame=(int)i, .x=(offset*i)});
        }
    }

    draw_statistics();
}

void DiffSortSelect::draw_level_select() {
    tex.draw_texture("diff_sort", "background", {.scale=(float)bg_resize->attribute, .center=true});
    if (confirmation) {
        tex.draw_texture("diff_sort", "star_select_prompt");
    } else {
        tex.draw_texture("diff_sort", "star_select_text", {.fade=diff_fade_in->attribute});
    }
    tex.draw_texture("diff_sort", "star_limit", {.frame=selected_box, .fade=diff_fade_in->attribute});
    tex.draw_texture("diff_sort", "level_box", {.fade=diff_fade_in->attribute});
    tex.draw_texture("diff_sort", "diff", {.frame=selected_box, .fade=diff_fade_in->attribute});
    tex.draw_texture("diff_sort", "star_num", {.frame=selected_level, .fade=diff_fade_in->attribute});
    for (size_t i = 0; i < selected_level; i++) {
        tex.draw_texture("diff_sort", "star", {.x=(float)(i*(40.5 * tex.screen_scale)), .fade=diff_fade_in->attribute});
    }

    if (confirmation) {
        TextureObject* texture = tex.textures["diff_sort"]["level_box"].get();
        ray::DrawRectangle(texture->x[0], texture->y[0], texture->x2[0], texture->y2[0], ray::Fade(ray::BLACK, 0.5));
        float y = -bounce_up_1->attribute + bounce_down_1->attribute - bounce_up_2->attribute + bounce_down_2->attribute;
        float offset = tex.skin_config["diff_sort_offset_2"].x;
        for (size_t i = 0; i < 3; i++) {
            if (i == confirm_index) {
                tex.draw_texture("diff_sort", "small_box_highlight", {.x=(i*offset), .y=y});
                tex.draw_texture("diff_sort", "small_box_text_highlight", {.frame=(int)i, .x=(i*offset), .y=y});
                tex.draw_texture("diff_sort", "small_box_outline", {.x=(i*offset), .y=y, .fade=box_flicker->attribute});
            } else {
                tex.draw_texture("diff_sort", "small_box", {.x=(i*offset), .y=y});
                tex.draw_texture("diff_sort", "small_box_text", {.frame=(int)i, .x=(i*offset), .y=y});
            }
        }
    } else {
        tex.draw_texture("diff_sort", "pongos");
        if (selected_level != 1) {
            tex.draw_texture("diff_sort", "arrow", {.x=(float)-blue_arrow_move->attribute, .fade=blue_arrow_fade->attribute, .index=0});
        }
        if (selected_level != limits[selected_box]) {
            tex.draw_texture("diff_sort", "arrow", {.mirror="horizontal", .x=(float)blue_arrow_move->attribute, .fade=blue_arrow_fade->attribute, .index=1});
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
