#pragma once
#include <array>

#include "../libs/gfx/color_utils.h"
#include "../libs/input/input.h"
#include "../objects/game/practice_menu.h"
#include "game.h"

class PracticeDrumHitEffect : public DrumHitEffect {
    int player_index;
    TextureObject* t_drum = nullptr;

public:
    PracticeDrumHitEffect(DrumType type, Side side, int player_index)
        : DrumHitEffect(type, side), player_index(player_index) {
        if (type == DrumType::DON) {
            t_drum = tex.get_texture("practice/large_drum_don");
        } else if (type == DrumType::KAT) {
            if (side == Side::LEFT) {
                t_drum = tex.get_texture("practice/large_drum_kat_l");
            } else if (side == Side::RIGHT) {
                t_drum = tex.get_texture("practice/large_drum_kat_r");
            }
        }
    }

    void draw(float y) override { tex.draw_texture(t_drum, {.fade = fade->attribute, .index = player_index}); }
};

class PracticePlayer : public Player {
public:
    bool paused = false;

    PracticePlayer(std::optional<SongParser>& parser_ref, PlayerNum player_num_param, int difficulty_param,
                   bool is_2p_param, const Modifiers& modifiers_param)
        : Player(parser_ref, player_num_param, difficulty_param, is_2p_param, modifiers_param) {
        judge_counter = JudgeCounter();
    }

    void handle_input(double ms_from_start, double current_ms, std::optional<Background>& background) override {
        if (paused) {
            return;
        }
        if (is_auto_play()) {
            while (true) {
                if (is_l_don_pressed(player_num)) {
                    spawn_hit_effects(DrumType::DON, Side::LEFT);
                } else if (is_r_don_pressed(player_num)) {
                    spawn_hit_effects(DrumType::DON, Side::RIGHT);
                } else if (is_l_kat_pressed(player_num)) {
                    spawn_hit_effects(DrumType::KAT, Side::LEFT);
                } else if (is_r_kat_pressed(player_num)) {
                    spawn_hit_effects(DrumType::KAT, Side::RIGHT);
                } else {
                    break;
                }
            }
            return;
        }
        Player::handle_input(ms_from_start, current_ms, background);
    }

    void spawn_hit_effects(DrumType drum_type, Side side) override {
        lane_hit_effect = LaneHitEffect(drum_type, Judgments::BAD); // judgment parameter workaround
        if (draw_drum_hit_list.size() < 4) {
            draw_drum_hit_list.push_back(std::make_unique<DrumHitEffect>(drum_type, side));
        }
        spawn_scrobble_effect(drum_type, side, static_cast<int>(player_num) - 1);
    }

    void spawn_scrobble_effect(DrumType drum_type, Side side, int player_index) {
        if (draw_drum_hit_list.size() < 4) {
            draw_drum_hit_list.push_back(std::make_unique<PracticeDrumHitEffect>(drum_type, side, player_index));
        }
    }
};

class PracticeGameScreen : public GameScreen {
public:
    PracticeGameScreen() : GameScreen("game") {}

    void on_screen_start() override;
    std::string background_scene_preset() const override { return "PRACTICE"; }
    Screens on_screen_end(Screens next_screen) override;
    void create_players() override;
    std::optional<Screens> update() override;
    void draw() override;

    // Check if practice mode is currently paused
    bool is_paused() const { return clock.is_paused(); }

    // Handle mouse/touch input for practice mode UI buttons
    std::optional<Screens> handle_mouse_input();

private:
    void release_chart_resources() override;
    float song_playback_rate() const override { return song_speed / 10.0f; }

    PracticePlayer* practice_player = nullptr; // non-owning, points into players[0]

    int scrobble_index = 0;
    double scrobble_time = 0;
    std::unique_ptr<MoveAnimation> scrobble_move;

    std::vector<Note> bars;
    std::vector<Note> scrobble_note_list;
    std::vector<double> markers;

    NoteList base_chart;
    std::vector<NoteList> branch_m_all, branch_e_all, branch_n_all;
    size_t branch_display_synced = 0;

    static constexpr int MIN_SONG_SPEED = 5;
    static constexpr int MAX_SONG_SPEED = 99;
    int song_speed = 10;

    enum class Phase {
        PRACTICE,
        LAST_PLAY,
        REVIEW
    };
    static constexpr int REVIEW_SECONDS = 30;
    Phase phase = Phase::PRACTICE;
    void start_last_play();
    void start_review();

    double now_ms = 0;
    double tempo_change_ms = -1e9;
    double seek_limit_ms = -1e9;
    double review_start_ms = -1e9;
    double max_progress = 0; // furthest point played, 0-1
    bool review_panel_open() const;

    TextureResizeAnimation* pause_don_anim;
    TextureResizeAnimation* pause_kat_anim;
    TextureResizeAnimation* resume_don_anim;
    TextureResizeAnimation* skip_l_kat_anim;
    TextureResizeAnimation* skip_r_kat_anim;
    TextureResizeAnimation* menu_don_anim;
    TextureResizeAnimation* speed_l_kat_anim;
    TextureResizeAnimation* speed_r_kat_anim;
    TextureResizeAnimation* mark_action_anim;
    TextureResizeAnimation* mark_finish_anim;

    std::array<int, PracticeMenu::MARK_SLOTS> jump_bars = {-1, -1, -1, -1, -1};
    PracticeMenu menu;

    int jump_arrow_bar = -1;
    std::unique_ptr<MoveAnimation> jump_arrow_anim;

    // Textures resolved once in on_screen_start(), after GameScreen::on_screen_start()'s
    // load_screen_textures() has run, instead of calling tex.get_texture() every frame.
    void init_practice_textures();

    TextureObject* t_notes[10] = {};
    TextureObject* t_notes_0 = nullptr;
    TextureObject* t_notes_8 = nullptr;
    TextureObject* t_notes_9 = nullptr;
    TextureObject* t_notes_10 = nullptr;
    TextureObject* t_drumroll_big_tail = nullptr;
    TextureObject* t_drumroll_tail = nullptr;
    TextureObject* t_moji = nullptr;
    TextureObject* t_moji_drumroll_mid = nullptr;

    TextureObject* t_large_drum = nullptr;
    TextureObject* t_pause_don = nullptr;
    TextureObject* t_pause_kat = nullptr;
    TextureObject* t_resume_don = nullptr;
    TextureObject* t_skip_l_kat = nullptr;
    TextureObject* t_skip_r_kat = nullptr;
    TextureObject* t_menu_don = nullptr;
    TextureObject* t_speed_r_kat = nullptr;
    TextureObject* t_speed_l_kat = nullptr;
    TextureObject* t_confirm = nullptr;
    TextureObject* t_delete = nullptr;
    TextureObject* t_finish = nullptr;
    TextureObject* t_jump_point_editing = nullptr;
    TextureObject* t_jump_point_arrow = nullptr;
    TextureObject* t_playing = nullptr;
    TextureObject* t_progress_bar_bg = nullptr;
    TextureObject* t_progress_bar = nullptr;
    TextureObject* t_gogo_marker = nullptr;
    TextureObject* t_jump_point_progress = nullptr;
    TextureObject* t_bar_count = nullptr;
    TextureObject* t_bar_divider = nullptr;
    TextureObject* t_bar_count_bar = nullptr;
    TextureObject* t_song_tempo = nullptr;
    TextureObject* t_dot = nullptr;
    TextureObject* t_multiplier = nullptr;
    TextureObject* t_bar_label = nullptr;
    TextureObject* t_paused = nullptr;
    TextureObject* t_tempo_rate = nullptr;
    TextureObject* t_tempo_dot = nullptr;
    TextureObject* t_tempo_dot_on = nullptr;
    TextureObject* t_skip_l_kat_off = nullptr;
    TextureObject* t_skip_r_kat_off = nullptr;
    TextureObject* t_menu_arrow = nullptr;
    TextureObject* t_menu_decide = nullptr;
    TextureObject* t_menu_panel = nullptr;
    TextureObject* t_jump_buttons[3] = {};
    TextureObject* t_review_cards[3] = {};
    TextureObject* t_gauge_max = nullptr;
    TextureObject* t_jump_count = nullptr;
    TextureObject* t_jump_count_digits = nullptr;
    TextureObject* t_seek_limit = nullptr;
    TextureObject* t_review_panel = nullptr;
    TextureObject* t_renda_bubbles[3] = {};
    TextureObject* t_renda_slash = nullptr;
    TextureObject* t_renda_digits = nullptr;
    TextureObject* t_renda_digits_small = nullptr;

    void initialize_practice_chart();
    void sync_branch_display();
    void pause_practice();
    void resume_practice();
    void open_practice_menu();
    void change_practice_speed(bool down, bool up);
    void restart_practice();
    std::optional<Screens> handle_menu_action(PracticeMenu::Action action);
    std::optional<Screens> global_keys_practice();
    void animate_scrobble_to(int new_index);
    void scrobble_step_bar(bool right);
    void review_jump(int card);
    bool at_first_bar() const { return scrobble_index <= 0; }
    bool at_last_bar() const { return scrobble_index >= std::ssize(bars) - 1; }

    float get_scrobble_position_x(const Note& note, double current_ms) const;
    Color moji_judgment_color(int note_index) const;
    void draw_bar_scrobble(const Note& bar, double current_ms) const;
    void draw_drumroll_scrobble(const Note& head, double current_ms) const;
    void draw_balloon_scrobble(const Note& head, double current_ms) const;
    void draw_notes_scrobble(double current_ms) const;
    const Note* find_tail(const Note& head) const;
    void draw_renda_marker(float cx, float cy, int count, int required) const;
    void draw_skip_buttons(int player_idx, bool clamp) const;
    void draw_tempo_feedback(int other_idx, bool dots) const;
    void draw_menu_arrows() const;
    void draw_review_panel() const;
};
