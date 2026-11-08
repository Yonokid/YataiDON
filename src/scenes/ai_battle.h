#pragma once

#include <array>
#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include "../objects/game/ai_battle_overlay.h"
#include "../objects/game/ai_match.h"
#include "../objects/result/ai_reward_flow.h"
#include "../objects/result/ai_section_result.h"
#include "../objects/song_select/ai_intro.h"
#include "../objects/song_select/ai_support_menu.h"
#include "game_2p.h"
#include "result.h"
#include "song_select.h"

static constexpr int AI_ASSIGNED_RANK = 10;

class AISongSelectScreen : public SongSelectScreen {
    std::optional<AISupportMenu> menu;
    std::unique_ptr<AIIntro> intro;
    std::unique_ptr<Chara3D> ai_chara;
    int chosen_course = 0;
    int level_cursor = 0;
    bool choosing_level = false;
    bool level_decided = false;

    AIBattleStatus status;

    int hovered_course() const;
    void play_call_voice();

protected:
    Screens get_game_screen_target() override { return Screens::AI_GAME; }
    bool hides_dan() override { return true; }
    fs::path bgm_override() override {
        const fs::path bgm = "Sounds/ai_select/bgm.ogg";
        return fs::exists(resolve_skin_path(bgm)) ? bgm : fs::path{}; // skins without it keep the normal select music
    }
    bool shows_ai_assigned() override { return status.answered && status.rank == AI_ASSIGNED_RANK; }
    bool allows_second_player_join() override { return false; }
    void select_song(SongBox* song) override;
    void handle_input_selecting() override;
    void draw_overlays() override;

public:
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};

class AIGameScreen : public Game2PScreen {
    std::optional<AIBattleOverlay> overlay;
    std::mt19937 rng{std::random_device{}()};
    AIBrain brain;

    std::vector<double> section_starts;
    std::unordered_map<int, int> note_section;
    std::array<std::vector<AITally>, 2> tallies;
    AITally player_total;
    int section_count = 0;
    int current_section = 0;
    int sections_won = 0;
    bool match_recorded = false;
    std::vector<bool> section_won;

    Player* human() const;
    Player* ai() const;
    bool overlay_active() const { return overlay && overlay->loaded(); }
    void start_match();
    void apply_overlay_layout();
    void update_tallies();
    void close_finished_sections();
    void record_match(const SessionData& session);
    double section_end_ms(int section) const;
    int current_territory() const;

protected:
    Screens select_screen() const override { return Screens::AI_SELECT; }
    Screens result_screen() const override { return Screens::AI_RESULT; }

public:
    Modifiers get_player_modifiers(PlayerNum pn) override;
    void initialize_chart(const fs::path& song) override;
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};

class AIResultScreen : public ResultScreen {
    std::optional<AISectionBoard> board;
    std::optional<AIRewardFlow> flow;
    bool board_started = false;

protected:
    Screens select_screen() const override { return Screens::AI_SELECT; }
    double reveal_end_ms() override;
    // confirm presses are consumed on read, so the base must not see them once the board has the drum
    void handle_input(double current_ms) override {
        if (!board_started) {
            ResultScreen::handle_input(current_ms);
        }
    }

public:
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
