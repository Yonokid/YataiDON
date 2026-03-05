#pragma once
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../libs/input.h"
#include "neiro.h"
#include "modifier.h"
#include "diff_sort.h"
//#include "../../libs/chara_2d.h"
#include "../global/nameplate.h"
#include "../enums.h"

class SongSelectPlayer {
public:
    PlayerNum player_num;
    Difficulty selected_difficulty;
    Difficulty prev_diff;
    bool selected_song;
    bool is_ready;
    bool is_ura;
    int ura_toggle;
    bool diff_select_move_right;
    std::string search_string;

    std::optional<NeiroSelector> neiro_selector;
    std::optional<ModifierSelector> modifier_selector;

    MoveAnimation* diff_selector_move_1;
    MoveAnimation* diff_selector_move_2;
    FadeAnimation* text_fade_in;
    MoveAnimation* selected_diff_bounce;
    FadeAnimation* selected_diff_fadein;
    FadeAnimation* selected_diff_highlight_fade;
    MoveAnimation* selected_diff_text_resize;
    FadeAnimation* selected_diff_text_fadein;

    //Chara2D chara;
    Nameplate nameplate;

    SongSelectPlayer(PlayerNum player_num, FadeAnimation* text_fade_in);

    void update(double current_time);
    bool is_voice_playing();
    //void on_song_selected(SongFile* selected_song);

    //std::string handle_input_browsing(double last_moved, SongFile* selected_item);
    std::optional<std::pair<int,int>> handle_input_diff_sort(DiffSortSelect* diff_sort_selector);
    std::optional<std::string> handle_input_search();
    //std::optional<std::string> handle_input_selected(SongFile* current_item);

    void draw_selector(bool is_half);
    void draw_background_diffs(SongSelectState state);
    void draw(SongSelectState state, bool is_half = false);

private:
    std::optional<std::string> navigate_difficulty_left(const std::vector<Difficulty>& diffs);
    std::optional<std::string> navigate_difficulty_right(const std::vector<Difficulty>& diffs);
    std::string toggle_ura_mode();
};
