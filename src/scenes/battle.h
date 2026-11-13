#pragma once

#include <deque>
#include <optional>
#include <random>

#include "../objects/game/battle_overlay.h"
#include "../objects/result/battle_result_board.h"
#include "../objects/song_select/battle_select_info.h"
#include "game.h"
#include "result.h"
#include "song_select.h"

class BattleSongSelectScreen : public SongSelectScreen {
    std::optional<BattleSelectInfo> info;

protected:
    Screens get_game_screen_target() override { return Screens::BATTLE_GAME; }
    bool hides_dan() override { return true; }
    bool allows_second_player_join() override { return false; }
    void draw_overlays() override;

public:
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
};

class BattleGameScreen : public GameScreen {
    std::optional<BattleOverlay> overlay;
    std::mt19937 rng{std::random_device{}()};

    int attack = 0;
    int difficulty = 0;
    int stage = 0;
    bool boss = false;
    int boss_hp = 0;
    std::deque<int> field;
    int enemy_hp = 0;
    int total_damage = 0;
    int skill_notes = 0;
    int skills_fired = 0;
    int rolls_seen = 0;
    int chests = 0;
    int kills = 0;
    std::optional<double> skill_lands_ms;
    bool ended = false;

    bool overlay_active() const { return overlay && overlay->loaded(); }
    void start_battle();
    void announce_battle();
    void apply_overlay_layout();
    void on_note(const Note& note, Judgments judgment);
    void deal(int damage, bool skill = false);
    void pop_next();
    void update_battle();
    void submit_song();

protected:
    Screens select_screen() const override { return Screens::BATTLE_SELECT; }
    Screens result_screen() const override { return Screens::BATTLE_RESULT; }

public:
    void initialize_chart(const fs::path& song) override;
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};

class BattleResultScreen : public ResultScreen {
    std::optional<BattleResultBoard> board;
    bool board_started = false;
    double opened_ms = 0;

protected:
    Screens select_screen() const override { return Screens::BATTLE_SELECT; }
    bool run_failed() const override { return false; }
    double reveal_end_ms() override { return board ? 0.0 : ResultScreen::reveal_end_ms(); }
    void handle_input(double current_ms) override {
        if (!board) {
            ResultScreen::handle_input(current_ms);
        }
    }

public:
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
