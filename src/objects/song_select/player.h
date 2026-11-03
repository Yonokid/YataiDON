#pragma once

#include "../../libs/gfx/texture.h"
#include "../global/chara_3d.h"
#include "../global/nameplate.h"
#include "diff_sort.h"
#include "modifier.h"
#include "neiro.h"
#include "ura_switch.h"

class SongSelectScript;

class SongSelectPlayer {
public:
    PlayerNum player_num;
    PlayerData player_data;
    Difficulty selected_difficulty;
    Difficulty prev_diff;
    std::vector<Difficulty> curr_diffs;
    double last_moved;
    double hold_start = 0, next_repeat = 0;
    int hold_dir = 0;
    double pending_select_ms = -1; // a don pressed while the wheel still scrolls, taken once it settles
    double last_browse_ms = 0;
    int open_frames = 0; // frames the current box has been open (drawn open by the skin)
    bool selected_song;
    bool is_ready;
    bool is_ura;
    int ura_toggle;
    bool diff_select_move_right;
    std::string search_string;

    bool search_guard = false;
    double search_guard_until_ms = 0.0;

    std::optional<NeiroSelector> neiro_selector;
    std::optional<ModifierSelector> modifier_selector;
    std::optional<UraSwitchAnimation> ura_switch;

    MoveAnimation* diff_selector_move_1;
    MoveAnimation* diff_selector_move_2;
    MoveAnimation* selected_diff_bounce;
    FadeAnimation* selected_diff_fadein;
    FadeAnimation* selected_diff_highlight_fade;
    TextureResizeAnimation* selected_diff_text_resize;
    FadeAnimation* selected_diff_text_fadein;

    std::unique_ptr<Chara3D> chara;
    Nameplate nameplate;

    SongSelectScript* script = nullptr;

    bool option_panel_by_lua = false;

    bool solo = true;
    double oni_open_ms = -1.0;

    SongSelectPlayer(PlayerNum player_num);

    void update(double current_time);
    bool is_voice_playing();

    bool difficulty_decided() const { return selected_difficulty >= Difficulty::EASY && (voice_played || is_ready); }

    SongSelectState select_song();
    void sync_ura(bool ura);
    void init_diff_cursor();
    void reset_selection();
    SongSelectState handle_input_browsing(double current_ms);
    SongSelectState handle_input_selecting();
    std::optional<std::pair<int, int>> handle_input_diff_sort(DiffSortSelect* diff_sort_selector);
    std::optional<std::string> handle_input_search();

    void try_lua_selector(bool is_half, float fade_in, int pass);
    void draw_background_diffs(SongSelectState state);
    void draw(SongSelectState state, bool is_half, float diff_fade_in);

private:
    bool voice_played;
    bool oni_hidden = false;
    int oni_command = 0;
    void hide_oni();
    void open_oni();
    void navigate_difficulty_left();
    void navigate_difficulty_right();
    void toggle_ura_mode();
    void start_background_diffs();
};
