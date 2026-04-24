#pragma once
#include "game.h"
#include "../objects/game/judge_counter.h"

class PracticePlayer : public Player {
public:
    bool paused = false;

    PracticePlayer(std::optional<SongParser>& parser_ref, PlayerNum player_num_param,
                   int difficulty_param, bool is_2p_param, const Modifiers& modifiers_param)
        : Player(parser_ref, player_num_param, difficulty_param, is_2p_param, modifiers_param) {
        gauge.reset();
        judge_counter = JudgeCounter();
    }
};

class PracticeGameScreen : public GameScreen {
public:
    PracticeGameScreen() : GameScreen("game") {}

    void on_screen_start() override;
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

    void init_tja_practice(const fs::path& song);
    void pause_song_practice();
    std::optional<Screens> global_keys_practice();

    float get_scrobble_position_x(const Note& note, double current_ms) const;
    void draw_bar_scrobble(const Note& bar, double current_ms) const;
    void draw_drumroll_scrobble(const Note& head, double current_ms) const;
    void draw_balloon_scrobble(const Note& head, double current_ms) const;
    void draw_notes_scrobble(double current_ms) const;
};
