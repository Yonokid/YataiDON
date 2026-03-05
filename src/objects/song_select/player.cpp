#include "player.h"

SongSelectPlayer::SongSelectPlayer(PlayerNum player_num, FadeAnimation* text_fade_in)
    : player_num(player_num), text_fade_in(text_fade_in)
      //chara(player_num - 1),
{
    NameplateConfig plate_info;
    if (player_num == PlayerNum::P2) {
        plate_info = global_data.config->nameplate_2p;
    } else {
        plate_info = global_data.config->nameplate_1p;
    }
    nameplate = Nameplate(plate_info.name, plate_info.title, player_num, plate_info.dan, plate_info.gold, plate_info.rainbow, plate_info.title_bg);

    selected_difficulty = Difficulty::BACK;
    prev_diff = Difficulty::BACK;
    selected_song = false;
    is_ready = false;
    is_ura = false;
    ura_toggle = 0;
    diff_select_move_right = false;

    diff_selector_move_1          = (MoveAnimation*)tex.get_animation(26, true);
    diff_selector_move_2          = (MoveAnimation*)tex.get_animation(27, true);
    selected_diff_bounce          = (MoveAnimation*)tex.get_animation(33, true);
    selected_diff_fadein          = (FadeAnimation*)tex.get_animation(34, true);
    selected_diff_highlight_fade  = (FadeAnimation*)tex.get_animation(35, true);
    selected_diff_text_resize     = (MoveAnimation*)tex.get_animation(36, true);
    selected_diff_text_fadein     = (FadeAnimation*)tex.get_animation(37, true);
}

void SongSelectPlayer::update(double current_time) {
    selected_diff_bounce->update(current_time);
    selected_diff_fadein->update(current_time);
    selected_diff_highlight_fade->update(current_time);
    selected_diff_text_resize->update(current_time);
    selected_diff_text_fadein->update(current_time);
    diff_selector_move_1->update(current_time);
    diff_selector_move_2->update(current_time);
    nameplate.update(current_time);
    //chara.update(current_time, 100, false, false);

    if (neiro_selector.has_value()) {
        neiro_selector->update(current_time);
        if (neiro_selector->is_finished) {
            neiro_selector.reset();
        }
    }

    if (modifier_selector.has_value()) {
        modifier_selector->update(current_time);
        if (modifier_selector->is_finished) {
            modifier_selector.reset();
        }
    }
}

bool SongSelectPlayer::is_voice_playing() {
    return audio->is_sound_playing("voice_start_song_" + std::to_string((int)player_num) + "p");
}

/*void SongSelectPlayer::on_song_selected(SongFile* song) {
    if (song->parser->metadata.course_data.find((int)Difficulty::URA) == song->parser->metadata.course_data.end()) {
        is_ura = false;
    } else if (song->parser->metadata.course_data.find((int)Difficulty::URA) != song->parser->metadata.course_data.end() &&
               song->parser->metadata.course_data.find((int)Difficulty::ONI) == song->parser->metadata.course_data.end()) {
        is_ura = true;
    }
    }*/

/*std::string SongSelectPlayer::handle_input_browsing(double last_moved, SongFile* selected_item) {
    double current_time = get_current_ms();

    if (ray::IsKeyPressed(ray::KEY_LEFT_CONTROL) || (is_l_kat_pressed(player_num) && current_time <= last_moved + 50)) {
        audio->play_sound("skip", "sound");
        return "skip_left";
    }
    if (ray::IsKeyPressed(ray::KEY_RIGHT_CONTROL) || (is_r_kat_pressed(player_num) && current_time <= last_moved + 50)) {
        audio->play_sound("skip", "sound");
        return "skip_right";
    }

    float wheel = ray::GetMouseWheelMove();
    if (is_l_kat_pressed(player_num) || wheel > 0) {
        audio->play_sound("kat", "sound");
        return "navigate_left";
    }
    if (is_r_kat_pressed(player_num) || wheel < 0) {
        audio->play_sound("kat", "sound");
        return "navigate_right";
    }

    if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
        if (selected_item != nullptr && dynamic_cast<BackBox*>(selected_item->box) != nullptr) {
            audio->play_sound("cancel", "sound");
            return "go_back";
        } else if (auto* dir = dynamic_cast<Directory*>(selected_item)) {
            if (dir->collection == Directory::COLLECTIONS[3]) return "diff_sort";
            if (dir->collection == Directory::COLLECTIONS[5]) return "search";
        }
        return "select_song";
    }

    if (ray::IsKeyPressed(ray::KEY_SPACE)) {
        audio->play_sound("add_favorite", "sound");
        return "add_favorite";
    }

    return "";
}*/

std::optional<std::pair<int,int>> SongSelectPlayer::handle_input_diff_sort(DiffSortSelect* diff_sort_selector) {
    if (is_l_kat_pressed(player_num)) {
        diff_sort_selector->input_left();
        audio->play_sound("kat", "sound");
    }
    if (is_r_kat_pressed(player_num)) {
        diff_sort_selector->input_right();
        audio->play_sound("kat", "sound");
    }
    if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
        audio->play_sound("don", "sound");
        return diff_sort_selector->input_select();
    }
    return std::nullopt;
}

std::optional<std::string> SongSelectPlayer::handle_input_search() {
    if (ray::IsKeyPressed(ray::KEY_BACKSPACE)) {
        if (!search_string.empty())
            search_string.pop_back();
    } else if (ray::IsKeyPressed(ray::KEY_ENTER)) {
        std::string result = search_string;
        search_string = "";
        return result;
    }

    int key = ray::GetCharPressed();
    while (key > 0) {
        search_string += (char)key;
        key = ray::GetCharPressed();
    }
    return std::nullopt;
}

/*std::optional<std::string> SongSelectPlayer::handle_input_selected(SongFile* current_item) {
    if (neiro_selector.has_value()) {
        if (is_l_kat_pressed(player_num))      neiro_selector->move_left();
        else if (is_r_kat_pressed(player_num)) neiro_selector->move_right();
        if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
            audio->play_sound("don", "sound");
            neiro_selector->confirm();
        }
        return std::nullopt;
    }

    if (modifier_selector.has_value()) {
        if (is_l_kat_pressed(player_num)) {
            audio->play_sound("kat", "sound");
            modifier_selector->left();
        } else if (is_r_kat_pressed(player_num)) {
            audio->play_sound("kat", "sound");
            modifier_selector->right();
        }
        if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
            audio->play_sound("don", "sound");
            modifier_selector->confirm();
        }
        return std::nullopt;
    }

    if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
        if (selected_difficulty == Difficulty::BACK) {
            return "cancel";
        } else if (selected_difficulty == Difficulty::MODIFIER) {
            audio->play_sound("don", "sound");
            modifier_selector = ModifierSelector(player_num);
            return std::nullopt;
        } else if (selected_difficulty == Difficulty::NEIRO) {
            audio->play_sound("don", "sound");
            neiro_selector = NeiroSelector(player_num);
            return std::nullopt;
        } else {
            is_ready = true;
            return "confirm";
        }
    }

    if (is_l_kat_pressed(player_num) || is_r_kat_pressed(player_num)) {
        audio->play_sound("kat", "sound");
        auto& course_data = current_item->parser->metadata.course_data;
        std::vector<Difficulty> diffs;
        for (const auto& [k, v] : course_data) diffs.push_back(Difficulty(k));
        std::sort(diffs.begin(), diffs.end());

        Difficulty prev = selected_difficulty;
        std::optional<std::string> ret;

        if (is_l_kat_pressed(player_num))      ret = navigate_difficulty_left(diffs);
        else if (is_r_kat_pressed(player_num)) ret = navigate_difficulty_right(diffs);

        if (selected_difficulty >= Difficulty::EASY && selected_difficulty <= Difficulty::URA
            && selected_difficulty != prev) {
            selected_diff_bounce->start();
            selected_diff_fadein->start();
        }
        return ret;
    }

    if (ray::IsKeyPressed(ray::KEY_TAB) &&
        (selected_difficulty == Difficulty::ONI || selected_difficulty == Difficulty::URA)) {
        return toggle_ura_mode();
    }

    return std::nullopt;
    }*/

std::optional<std::string> SongSelectPlayer::navigate_difficulty_left(const std::vector<Difficulty>& diffs) {
    diff_select_move_right = false;

    if (is_ura && selected_difficulty == Difficulty::URA) {
        diff_selector_move_1->start();
        prev_diff = selected_difficulty;
        selected_difficulty = (diffs.size() == 1) ? Difficulty::NEIRO : diffs[diffs.size() - 3];
    } else if (selected_difficulty == Difficulty::NEIRO || selected_difficulty == Difficulty::MODIFIER) {
        diff_selector_move_2->start();
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty((int)selected_difficulty - 1);
    } else if (selected_difficulty == Difficulty::BACK) {
        // no-op
    } else if (std::find(diffs.begin(), diffs.end(), selected_difficulty) == diffs.end()) {
        prev_diff = selected_difficulty;
        diff_selector_move_1->start();
        selected_difficulty = diffs.front();
    } else if (selected_difficulty == diffs.front()) {
        diff_selector_move_2->start();
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty::NEIRO;
    } else {
        diff_selector_move_1->start();
        prev_diff = selected_difficulty;
        auto it = std::find(diffs.begin(), diffs.end(), selected_difficulty);
        selected_difficulty = *std::prev(it);
    }
    return std::nullopt;
}

std::optional<std::string> SongSelectPlayer::navigate_difficulty_right(const std::vector<Difficulty>& diffs) {
    diff_select_move_right = true;

    if (is_ura && selected_difficulty == Difficulty::HARD) {
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty::URA;
        diff_selector_move_1->start();
    }

    bool has_ura = std::find(diffs.begin(), diffs.end(), Difficulty::URA) != diffs.end();
    bool has_oni = std::find(diffs.begin(), diffs.end(), Difficulty::ONI) != diffs.end();

    if ((selected_difficulty == Difficulty::ONI || selected_difficulty == Difficulty::URA) && has_ura && has_oni) {
        ura_toggle = (ura_toggle + 1) % 10;
        if (ura_toggle == 0) return toggle_ura_mode();
    } else if (selected_difficulty == Difficulty::NEIRO) {
        prev_diff = selected_difficulty;
        selected_difficulty = diffs.front();
        diff_selector_move_2->start();
        diff_selector_move_1->start();
    } else if (selected_difficulty == Difficulty::MODIFIER || selected_difficulty == Difficulty::BACK) {
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty((int)selected_difficulty + 1);
        diff_selector_move_2->start();
    } else if (selected_difficulty < diffs.back()) {
        prev_diff = selected_difficulty;
        auto it = std::find(diffs.begin(), diffs.end(), selected_difficulty);
        selected_difficulty = *std::next(it);
        diff_selector_move_1->start();
    }
    return std::nullopt;
}

std::string SongSelectPlayer::toggle_ura_mode() {
    ura_toggle = 0;
    is_ura = !is_ura;
    audio->play_sound("ura_switch", "sound");
    selected_difficulty = Difficulty(7 - (int)selected_difficulty);
    return "ura_toggle";
}

void SongSelectPlayer::draw_selector(bool is_half) {
    float fade = (neiro_selector.has_value() || modifier_selector.has_value())
        ? 0.5f : text_fade_in->attribute;
    float direction = diff_select_move_right ? 1.0f : -1.0f;
    float offset = tex.skin_config["selector_offset"].x;
    float balloon_offset_1 = tex.skin_config["selector_balloon_offset_1"].x;
    float balloon_offset_2 = tex.skin_config["selector_balloon_offset_2"].x;

    std::string p = std::to_string((int)player_num) + "p";
    std::string half_suffix = is_half ? "_half" : "";

    if (selected_difficulty <= Difficulty::NEIRO || prev_diff == Difficulty::NEIRO) {
        if (prev_diff == Difficulty::NEIRO && selected_difficulty >= Difficulty::EASY) {
            if (!diff_selector_move_2->is_finished) {
                float bx = (((int)prev_diff + 3) * balloon_offset_2) + balloon_offset_1 + (diff_selector_move_2->attribute * direction);
                tex.draw_texture("diff_select", p + "_balloon" + half_suffix,      {.x=bx, .fade=fade});
                tex.draw_texture("diff_select", p + "_outline_back" + half_suffix, {.x=(((int)prev_diff + 3) * balloon_offset_2) + ((float)diff_selector_move_2->attribute * direction)});
            } else {
                Difficulty difficulty = std::min(Difficulty::ONI, selected_difficulty);
                tex.draw_texture("diff_select", p + "_balloon" + half_suffix, {.x=((int)difficulty * offset), .fade=fade});
                tex.draw_texture("diff_select", p + "_outline" + half_suffix, {.x=((int)difficulty * offset)});
            }
        } else if (!diff_selector_move_2->is_finished) {
            tex.draw_texture("diff_select", p + "_outline_back" + half_suffix, {.x=(((int)prev_diff + 3) * balloon_offset_2) + ((float)diff_selector_move_2->attribute * direction)});
            if (selected_difficulty != Difficulty::BACK) {
                float bx = (((int)prev_diff + 3) * balloon_offset_2) + balloon_offset_1 + (diff_selector_move_2->attribute * direction);
                tex.draw_texture("diff_select", p + "_balloon" + half_suffix, {.x=bx, .fade=fade});
            }
        } else {
            tex.draw_texture("diff_select", p + "_outline_back" + half_suffix, {.x=(((int)selected_difficulty + 3) * balloon_offset_2)});
            if ((int)selected_difficulty != -3) {
                float bx = (((int)selected_difficulty + 3) * balloon_offset_2) + balloon_offset_1;
                tex.draw_texture("diff_select", p + "_balloon" + half_suffix, {.x=bx, .fade=fade});
            }
        }
    } else {
        if (prev_diff == Difficulty::NEIRO) return;
        if (!diff_selector_move_1->is_finished) {
            Difficulty difficulty = std::min(Difficulty::ONI, prev_diff);
            float bx = ((int)difficulty * offset) + (diff_selector_move_1->attribute * direction);
            tex.draw_texture("diff_select", p + "_balloon" + half_suffix, {.x=bx, .fade=fade});
            tex.draw_texture("diff_select", p + "_outline" + half_suffix, {.x=bx});
        } else {
            Difficulty difficulty = std::min(Difficulty::ONI, selected_difficulty);
            tex.draw_texture("diff_select", p + "_balloon" + half_suffix, {.x=((int)difficulty * offset), .fade=fade});
            tex.draw_texture("diff_select", p + "_outline" + half_suffix, {.x=((int)difficulty * offset)});
        }
    }
}

void SongSelectPlayer::draw_background_diffs(SongSelectState state) {
    if (!selected_song || state != SongSelectState::SONG_SELECTED || selected_difficulty < Difficulty::EASY)
        return;

    float x_offset = ((int)player_num == 2) ? 1025.0f * tex.screen_scale : 0.0f;
    float bounce_y  = -selected_diff_bounce->attribute;
    float bounce_y2 =  selected_diff_bounce->attribute;
    int diff_frame     = (int)selected_difficulty;
    int diff_frame_oni = (int)(std::min(Difficulty::ONI, selected_difficulty));

    tex.draw_texture("global", "background_diff", {.frame=diff_frame, .x=x_offset, .y=bounce_y,  .y2=bounce_y2, .fade=std::min(0.5f, (float)selected_diff_fadein->attribute)});
    if (selected_diff_highlight_fade->is_reversing || selected_diff_highlight_fade->is_finished)
        tex.draw_texture("global", "background_diff", {.frame=diff_frame, .x=x_offset, .y=bounce_y, .y2=bounce_y2});
    tex.draw_texture("global", "background_diff_highlight",  {.frame=diff_frame_oni, .x=x_offset, .fade=selected_diff_highlight_fade->attribute});
    tex.draw_texture("global", "bg_diff_text_bg", {.scale=(float)selected_diff_text_resize->attribute, .center=true, .x=x_offset, .fade=std::min(0.5f, (float)selected_diff_text_fadein->attribute)});
    tex.draw_texture("global", "bg_diff_text",    {.frame=diff_frame_oni, .scale=(float)selected_diff_text_resize->attribute, .center=true, .x=x_offset, .fade=selected_diff_text_fadein->attribute});
}

void SongSelectPlayer::draw(SongSelectState state, bool is_half) {
    if (selected_song && state == SongSelectState::SONG_SELECTED)
        draw_selector(is_half);

    float offset = 0.0f;
    if (neiro_selector.has_value()) {
        offset = neiro_selector->move->attribute;
        offset = neiro_selector->is_confirmed
            ? offset + tex.skin_config["song_select_offset"].x
            : -offset;
    }
    if (modifier_selector.has_value()) {
        offset = modifier_selector->move->attribute;
        offset = modifier_selector->is_confirmed
            ? offset + tex.skin_config["song_select_offset"].x
            : -offset;
    }

    if ((int)player_num == 1) {
        nameplate.draw(tex.skin_config["song_select_nameplate_1p"].x, tex.skin_config["song_select_nameplate_1p"].y);
        //chara.draw({.x=tex.skin_config["song_select_chara_1p"].x, .y=tex.skin_config["song_select_chara_1p"].y + (offset * 0.6f)});
    } else {
        nameplate.draw(tex.skin_config["song_select_nameplate_2p"].x, tex.skin_config["song_select_nameplate_2p"].y);
        //chara.draw({.mirror=true, .x=tex.skin_config["song_select_chara_2p"].x, .y=tex.skin_config["song_select_chara_2p"].y + (offset * 0.6f)});
    }

    if (neiro_selector.has_value())   neiro_selector->draw();
    if (modifier_selector.has_value()) modifier_selector->draw();
}
