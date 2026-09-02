#pragma once
#include "game.h"
#include "../libs/input.h"
#include "../objects/game/practice_menu.h"

class PracticeDrumHitEffect : public DrumHitEffect {
    int player_index;
public:
    PracticeDrumHitEffect(DrumType type, Side side, int player_index)
        : DrumHitEffect(type, side), player_index(player_index) {}

    void draw(float y) override {
        if (type == DrumType::DON) {
            tex.draw_texture(PRACTICE::LARGE_DRUM_DON, {.fade = fade->attribute, .index = player_index});
        } else if (type == DrumType::KAT) {
            if (side == Side::LEFT)
                tex.draw_texture(PRACTICE::LARGE_DRUM_KAT_L, {.fade = fade->attribute, .index = player_index});
            else if (side == Side::RIGHT)
                tex.draw_texture(PRACTICE::LARGE_DRUM_KAT_R, {.fade = fade->attribute, .index = player_index});
        }
    }
};

class PracticePlayer : public Player {
public:
    bool paused = false;

    PracticePlayer(std::optional<SongParser>& parser_ref, PlayerNum player_num_param,
                   int difficulty_param, bool is_2p_param, const Modifiers& modifiers_param)
        : Player(parser_ref, player_num_param, difficulty_param, is_2p_param, modifiers_param) {
        judge_counter = JudgeCounter();
    }

    void handle_input(double ms_from_start, double current_ms, std::optional<Background>& background) override {
        if (paused) return;
        if (is_auto_play()) {
            while (true) {
                if      (is_l_don_pressed(player_num)) spawn_hit_effects(DrumType::DON, Side::LEFT);
                else if (is_r_don_pressed(player_num)) spawn_hit_effects(DrumType::DON, Side::RIGHT);
                else if (is_l_kat_pressed(player_num)) spawn_hit_effects(DrumType::KAT, Side::LEFT);
                else if (is_r_kat_pressed(player_num)) spawn_hit_effects(DrumType::KAT, Side::RIGHT);
                else break;
            }
            return;
        }
        Player::handle_input(ms_from_start, current_ms, background);
    }

    void spawn_hit_effects(DrumType drum_type, Side side) override {
        lane_hit_effect = LaneHitEffect(drum_type, Judgments::BAD); //judgment parameter workaround
        draw_drum_hit_list.push_back(std::make_unique<DrumHitEffect>(drum_type, side));
        spawn_scrobble_effect(drum_type, side, (int)player_num - 1);
    }

    void spawn_scrobble_effect(DrumType drum_type, Side side, int player_index) {
        draw_drum_hit_list.push_back(std::make_unique<PracticeDrumHitEffect>(drum_type, side, player_index));
    }
};

class PracticeGameScreen : public GameScreen {
public:
    PracticeGameScreen() : GameScreen("game") {}

    void on_screen_start() override;
    std::string background_scene_preset() const override { return "PRACTICE"; }
    Screens on_screen_end(Screens next_screen) override;
    void init_tja(fs::path song) override;
    std::optional<Screens> update() override;
    void draw() override;

private:
    PracticePlayer* practice_player = nullptr; // non-owning, points into players[0]

    int scrobble_index = 0;
    double scrobble_time = 0;
    std::unique_ptr<MoveAnimation> scrobble_move;

    std::vector<Note> bars;
    std::vector<Note> scrobble_note_list;
    std::vector<double> markers;

    int song_speed = 10;

    TextureResizeAnimation* pause_don_anim;
    TextureResizeAnimation* pause_kat_anim;
    TextureResizeAnimation* resume_don_anim;
    TextureResizeAnimation* skip_l_kat_anim;
    TextureResizeAnimation* skip_r_kat_anim;
    TextureResizeAnimation* menu_don_anim;
    TextureResizeAnimation* speed_l_kat_anim;
    TextureResizeAnimation* speed_r_kat_anim;

    int jump_bar = -1;
    PracticeMenu menu;

    void init_tja_practice(const fs::path& song);
    void pause_song_practice();
    void restart_practice();
    std::optional<Screens> handle_menu_action(PracticeMenu::Action action);
    std::optional<Screens> global_keys_practice();

    float get_scrobble_position_x(const Note& note, double current_ms) const;
    void draw_bar_scrobble(const Note& bar, double current_ms) const;
    void draw_drumroll_scrobble(const Note& head, double current_ms) const;
    void draw_balloon_scrobble(const Note& head, double current_ms) const;
    void draw_notes_scrobble(double current_ms) const;
};
