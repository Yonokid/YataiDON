#pragma once
#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../libs/input.h"
#include "neiro.h"
#include "modifier.h"
#include "diff_sort.h"
#include "ura_switch.h"
//#include "../../libs/chara_2d.h"
#include "../global/nameplate.h"
#include "../enums.h"

class SongSelectPlayer {
public:
    PlayerNum player_num;
    Difficulty selected_difficulty;
    Difficulty prev_diff;
    std::vector<Difficulty> curr_diffs;
    double last_moved;
    bool selected_song;
    bool is_ready;
    bool is_ura;
    int ura_toggle;
    bool diff_select_move_right;
    std::string search_string;

    std::optional<NeiroSelector> neiro_selector;
    std::optional<ModifierSelector> modifier_selector;
    std::optional<UraSwitchAnimation> ura_switch;

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

    SongSelectPlayer(PlayerNum player_num);

    void update(double current_time);
    bool is_voice_playing();
    //void on_song_selected(SongFile* selected_song);

    SongSelectState select_song();
    SongSelectState handle_input_browsing(double current_ms);
    SongSelectState handle_input_selecting();
    std::optional<std::pair<int,int>> handle_input_diff_sort(DiffSortSelect* diff_sort_selector);
    std::optional<std::string> handle_input_search();

    void draw_selector(bool is_half);
    void draw_background_diffs(SongSelectState state);
    void draw(SongSelectState state, bool is_half = false);

private:
    void navigate_difficulty_left();
    void navigate_difficulty_right();
    void toggle_ura_mode();
    void start_background_diffs();
};
