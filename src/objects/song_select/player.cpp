#include "player.h"
#include "../../libs/audio_engine.h"
#include "../../libs/input.h"

SongSelectPlayer::SongSelectPlayer(PlayerNum player_num)
    : player_num(player_num)
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
    last_moved = 0;

    diff_selector_move_1          = (MoveAnimation*)tex.get_animation(26, true);
    diff_selector_move_2          = (MoveAnimation*)tex.get_animation(27, true);
    selected_diff_bounce          = (MoveAnimation*)tex.get_animation(33, true);
    selected_diff_fadein          = (FadeAnimation*)tex.get_animation(34, true);
    selected_diff_highlight_fade  = (FadeAnimation*)tex.get_animation(35, true);
    selected_diff_text_resize     = (TextureResizeAnimation*)tex.get_animation(36, true);
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
    if (ura_switch.has_value()) ura_switch->update(current_time);
}

bool SongSelectPlayer::is_voice_playing() {
    return audio->is_sound_playing("voice_start_song_" + std::to_string((int)player_num) + "p");
}

void SongSelectPlayer::start_background_diffs() {
    selected_diff_text_resize->start();
    selected_diff_text_fadein->start();
    selected_diff_highlight_fade->start();
}

SongSelectState SongSelectPlayer::select_song() {
    audio->play_sound("don", "sound");
    BaseBox* item = navigator.get_current_item();
    if (navigator.is_directory(item) && item->genre_index == GenreIndex::DAN) {
        return SongSelectState::DAN_SELECTED;
    } else if (navigator.is_song(item)) {
        navigator.enter_diff_select();
        selected_song = true;
        SongBox* song_item = (SongBox*)item;
        curr_diffs = song_item->get_diffs();
        selected_diff_bounce->start();
        selected_diff_fadein->start();
        return SongSelectState::SONG_SELECTED;
    } else if (navigator.is_directory(item)) {
        navigator.load_current_directory(item->path);
    }
    return SongSelectState::BROWSING;
}

SongSelectState SongSelectPlayer::handle_input_browsing(double current_ms) {

    bool l_kat = is_l_kat_pressed(player_num);
    bool r_kat = is_r_kat_pressed(player_num);
    bool l_don = is_l_don_pressed(player_num);
    bool r_don = is_r_don_pressed(player_num);
    float wheel = ray::GetMouseWheelMove();

    if (ray::IsKeyPressed(ray::KEY_F5)) {
        navigator.load_current_directory(navigator.current_path);
    }

    if (ray::IsKeyPressed(ray::KEY_LEFT_CONTROL) || (l_kat && current_ms <= last_moved + 50)) {
        audio->play_sound("skip", "sound");
        navigator.skip_left();
        last_moved = current_ms;
    } else if (l_kat || wheel > 0) {
        audio->play_sound("kat", "sound");
        navigator.move_left();
        last_moved = current_ms;
    }

    if (ray::IsKeyPressed(ray::KEY_RIGHT_CONTROL) || (r_kat && current_ms <= last_moved + 50)) {
        audio->play_sound("skip", "sound");
        navigator.skip_right();
        last_moved = current_ms;
    } else if (r_kat || wheel < 0) {
        audio->play_sound("kat", "sound");
        navigator.move_right();
        last_moved = current_ms;
    }

    if (ray::IsKeyPressed(ray::KEY_SPACE)) {
        BaseBox* item = navigator.get_current_item();
        if (navigator.is_song(item)) {
            navigator.toggle_favorite(static_cast<SongBox*>(item));
            audio->play_sound("add_favorite", "sound");
        }
    }

    if (l_don || r_don) {
        BaseBox* item = navigator.get_current_item();
        if (navigator.is_directory(item) && item->collection == COLLECTIONS[5])
            return SongSelectState::SEARCHING;
        return select_song();
    }
    return SongSelectState::BROWSING;
}

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
        clear_input_buffers();
        return result;
    }

    int key = ray::GetCharPressed();
    while (key > 0) {
        search_string += (char)key;
        key = ray::GetCharPressed();
    }
    return std::nullopt;
}

SongSelectState SongSelectPlayer::handle_input_selecting() {
    bool l_kat = is_l_kat_pressed(player_num);
    bool r_kat = is_r_kat_pressed(player_num);
    bool l_don = is_l_don_pressed(player_num);
    bool r_don = is_r_don_pressed(player_num);

    if (l_kat) {
        if (modifier_selector.has_value()) {
            audio->play_sound("kat", "sound");
            modifier_selector->left();
        } else if (neiro_selector.has_value()) {
            neiro_selector->left();
        } else {
            audio->play_sound("kat", "sound");
            navigate_difficulty_left();
            if (selected_difficulty >= Difficulty::EASY) {
                selected_diff_bounce->start();
                selected_diff_fadein->start();
            }
        }
    } else if (r_kat) {
        if (modifier_selector.has_value()) {
            audio->play_sound("kat", "sound");
            modifier_selector->right();
        } else if (neiro_selector.has_value()) {
            neiro_selector->right();
        } else {
            audio->play_sound("kat", "sound");
            navigate_difficulty_right();
            if (selected_difficulty >= Difficulty::EASY) {
                selected_diff_bounce->start();
                selected_diff_fadein->start();
            }
        }
    } else if (l_don || r_don) {
        audio->play_sound("don", "sound");
        if (modifier_selector.has_value()) {
            modifier_selector->confirm();
        } else if (neiro_selector.has_value()) {
            neiro_selector->confirm();
        } else {
            if (selected_difficulty == Difficulty::MODIFIER) {
                modifier_selector = ModifierSelector(player_num);
            } else if (selected_difficulty == Difficulty::NEIRO) {
                neiro_selector = NeiroSelector(player_num);
            } else if (selected_difficulty >= Difficulty::EASY || selected_difficulty == Difficulty::BACK) {
                is_ready = true;
                start_background_diffs();
            }
        }
    }
    return SongSelectState::SONG_SELECTED;
}

void SongSelectPlayer::navigate_difficulty_left() {
    diff_select_move_right = false;

    if (is_ura && selected_difficulty == Difficulty::URA) {
        diff_selector_move_1->start();
        prev_diff = selected_difficulty;
        selected_difficulty = (curr_diffs.size() == 1) ? Difficulty::NEIRO : curr_diffs[curr_diffs.size() - 3];
    } else if (selected_difficulty == Difficulty::NEIRO || selected_difficulty == Difficulty::MODIFIER) {
        diff_selector_move_2->start();
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty((int)selected_difficulty - 1);
    } else if (selected_difficulty == Difficulty::BACK) {
        // no-op
    } else if (std::find(curr_diffs.begin(), curr_diffs.end(), selected_difficulty) == curr_diffs.end()) {
        prev_diff = selected_difficulty;
        diff_selector_move_1->start();
        selected_difficulty = curr_diffs.front();
    } else if (selected_difficulty == curr_diffs.front()) {
        diff_selector_move_2->start();
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty::NEIRO;
    } else {
        diff_selector_move_1->start();
        prev_diff = selected_difficulty;
        auto it = std::find(curr_diffs.begin(), curr_diffs.end(), selected_difficulty);
        selected_difficulty = *std::prev(it);
    }
}

void SongSelectPlayer::navigate_difficulty_right() {
    diff_select_move_right = true;

    if (is_ura && selected_difficulty == Difficulty::HARD) {
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty::URA;
        diff_selector_move_1->start();
    }

    bool has_ura = std::find(curr_diffs.begin(), curr_diffs.end(), Difficulty::URA) != curr_diffs.end();
    bool has_oni = std::find(curr_diffs.begin(), curr_diffs.end(), Difficulty::ONI) != curr_diffs.end();

    if ((selected_difficulty == Difficulty::ONI || selected_difficulty == Difficulty::URA) && has_ura && has_oni) {
        ura_toggle = (ura_toggle + 1) % 10;
        if (ura_toggle == 0) toggle_ura_mode();
    } else if (selected_difficulty == Difficulty::NEIRO) {
        prev_diff = selected_difficulty;
        selected_difficulty = curr_diffs.front();
        diff_selector_move_2->start();
        diff_selector_move_1->start();
    } else if (selected_difficulty == Difficulty::MODIFIER || selected_difficulty == Difficulty::BACK) {
        prev_diff = selected_difficulty;
        selected_difficulty = Difficulty((int)selected_difficulty + 1);
        diff_selector_move_2->start();
    } else if (selected_difficulty < curr_diffs.back()) {
        prev_diff = selected_difficulty;
        auto it = std::find(curr_diffs.begin(), curr_diffs.end(), selected_difficulty);
        selected_difficulty = *std::next(it);
        diff_selector_move_1->start();
    }
}

void SongSelectPlayer::toggle_ura_mode() {
    ura_toggle = 0;
    is_ura = !is_ura;
    audio->play_sound("ura_switch", "sound");
    selected_difficulty = Difficulty(7 - (int)selected_difficulty);
    ura_switch.emplace();
    ura_switch->start(is_ura);
}

void SongSelectPlayer::draw_selector(bool is_half) {
    float fade = (neiro_selector.has_value() || modifier_selector.has_value())
        ? 0.5f : selected_diff_fadein->attribute;
    float direction = diff_select_move_right ? 1.0f : -1.0f;
    float offset = tex.skin_config[SC::SELECTOR_OFFSET].x;
    float balloon_offset_1 = tex.skin_config[SC::SELECTOR_BALLOON_OFFSET_1].x;
    float balloon_offset_2 = tex.skin_config[SC::SELECTOR_BALLOON_OFFSET_2].x;

    std::string p = std::to_string((int)player_num) + "p";
    std::string half_suffix = is_half ? "_half" : "";

    if (selected_difficulty <= Difficulty::NEIRO || prev_diff == Difficulty::NEIRO) {
        if (prev_diff == Difficulty::NEIRO && selected_difficulty >= Difficulty::EASY) {
            if (!diff_selector_move_2->is_finished) {
                float bx = (((int)prev_diff + 3) * balloon_offset_2) + balloon_offset_1 + (diff_selector_move_2->attribute * direction);
                tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_balloon" + half_suffix)),      {.x=bx, .fade=fade});
                tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_outline_back" + half_suffix)), {.x=(((int)prev_diff + 3) * balloon_offset_2) + ((float)diff_selector_move_2->attribute * direction)});
            } else {
                Difficulty difficulty = std::min(Difficulty::ONI, selected_difficulty);
                tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_balloon" + half_suffix)), {.x=((int)difficulty * offset), .fade=fade});
                tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_outline" + half_suffix)), {.x=((int)difficulty * offset)});
            }
        } else if (!diff_selector_move_2->is_finished) {
            tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_outline_back" + half_suffix)), {.x=(((int)prev_diff + 3) * balloon_offset_2) + ((float)diff_selector_move_2->attribute * direction)});
            if (selected_difficulty != Difficulty::BACK) {
                float bx = (((int)prev_diff + 3) * balloon_offset_2) + balloon_offset_1 + (diff_selector_move_2->attribute * direction);
                tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_balloon" + half_suffix)), {.x=bx, .fade=fade});
            }
        } else {
            tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_outline_back" + half_suffix)), {.x=(((int)selected_difficulty + 3) * balloon_offset_2)});
            if ((int)selected_difficulty != -3) {
                float bx = (((int)selected_difficulty + 3) * balloon_offset_2) + balloon_offset_1;
                tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_balloon" + half_suffix)), {.x=bx, .fade=fade});
            }
        }
    } else {
        if (prev_diff == Difficulty::NEIRO) return;
        if (!diff_selector_move_1->is_finished) {
            Difficulty difficulty = std::min(Difficulty::ONI, prev_diff);
            float bx = ((int)difficulty * offset) + (diff_selector_move_1->attribute * direction);
            tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_balloon" + half_suffix)), {.x=bx, .fade=fade});
            tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_outline" + half_suffix)), {.x=bx});
        } else {
            Difficulty difficulty = std::min(Difficulty::ONI, selected_difficulty);
            tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_balloon" + half_suffix)), {.x=((int)difficulty * offset), .fade=fade});
            tex.draw_texture(tex_id_map.at("diff_select/" + (p + "_outline" + half_suffix)), {.x=((int)difficulty * offset)});
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

    tex.draw_texture(GLOBAL::BACKGROUND_DIFF, {.frame=diff_frame, .x=x_offset, .y=bounce_y,  .y2=bounce_y2, .fade=std::min(0.5f, (float)selected_diff_fadein->attribute)});
    if (selected_diff_highlight_fade->is_reversing || selected_diff_highlight_fade->is_finished)
        tex.draw_texture(GLOBAL::BACKGROUND_DIFF, {.frame=diff_frame, .x=x_offset, .y=bounce_y, .y2=bounce_y2});
    tex.draw_texture(GLOBAL::BACKGROUND_DIFF_HIGHLIGHT,  {.frame=diff_frame_oni, .x=x_offset, .fade=selected_diff_highlight_fade->attribute});
    tex.draw_texture(GLOBAL::BG_DIFF_TEXT_BG, {.scale=(float)selected_diff_text_resize->attribute, .center=true, .x=x_offset, .fade=std::min(0.5f, (float)selected_diff_text_fadein->attribute)});
    tex.draw_texture(GLOBAL::BG_DIFF_TEXT,    {.frame=diff_frame_oni, .scale=(float)selected_diff_text_resize->attribute, .center=true, .x=x_offset, .fade=selected_diff_text_fadein->attribute});
}

void SongSelectPlayer::draw(SongSelectState state, bool is_half) {
    if (selected_song && state == SongSelectState::SONG_SELECTED) {
        draw_selector(is_half);
    }

    float offset = 0.0f;
    if (neiro_selector.has_value()) {
        offset = neiro_selector->move->attribute;
        offset = neiro_selector->is_confirmed
            ? offset + tex.skin_config[SC::SONG_SELECT_OFFSET].x
            : -offset;
    }
    if (modifier_selector.has_value()) {
        offset = modifier_selector->move->attribute;
        offset = modifier_selector->is_confirmed
            ? offset + tex.skin_config[SC::SONG_SELECT_OFFSET].x
            : -offset;
    }

    if (player_num == PlayerNum::P1) {
        nameplate.draw(tex.skin_config[SC::SONG_SELECT_NAMEPLATE_1P].x, tex.skin_config[SC::SONG_SELECT_NAMEPLATE_1P].y);
        //chara.draw({.x=tex.skin_config["song_select_chara_1p"].x, .y=tex.skin_config["song_select_chara_1p"].y + (offset * 0.6f)});
    } else {
        nameplate.draw(tex.skin_config[SC::SONG_SELECT_NAMEPLATE_2P].x, tex.skin_config[SC::SONG_SELECT_NAMEPLATE_2P].y);
        //chara.draw({.mirror=true, .x=tex.skin_config["song_select_chara_2p"].x, .y=tex.skin_config["song_select_chara_2p"].y + (offset * 0.6f)});
    }

    if (neiro_selector.has_value())   neiro_selector->draw();
    if (modifier_selector.has_value()) modifier_selector->draw();
    if (ura_switch.has_value()) ura_switch->draw();
}
