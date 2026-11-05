#pragma once

#include <array>
#include <memory>
#include <optional>

#include "../objects/global/chara_3d.h"
#include "../objects/waiwai/waiwai_overlay.h"
#include "game_2p.h"
#include "result.h"
#include "retry_game.h"
#include "song_select_2p.h"

class WaiwaiSongSelectScreen : public SongSelect2PScreen {
    std::optional<WaiwaiOverlay> overlay;
    int don_hits = 0;

protected:
    Screens get_game_screen_target() override { return Screens::WAIWAI_GAME; }
    bool is_waiwai_screen() override { return true; }
    fs::path bgm_override() override { return "Sounds/waiwai/waienso/jingle_waigenre_1.ogg"; }
    void select_song(SongBox* song) override;
    void handle_input(double current_ms) override;

public:
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};

class WaiwaiTutorialScreen : public Screen {
    std::optional<WaiwaiOverlay> overlay;
    double start_ms = 0;
    int kat_hits = 0;

public:
    WaiwaiTutorialScreen() : Screen("waiwai_tutorial") {}
    bool two_player_audio() const override { return true; }
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};

class WaiwaiGameScreen : public Game2PScreen {
    std::optional<WaiwaiOverlay> overlay;

    void apply_shared_art();

protected:
    Screens select_screen() const override { return Screens::WAIWAI_SONG_SELECT; }
    Screens result_screen() const override { return Screens::WAIWAI_RESULT; }
    std::string background_scene_preset() const override { return "WAIWAI_" + scene_preset; }
    void draw_players() override;

public:
    void initialize_chart(const fs::path& song) override;
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
};

class WaiwaiResultScreen : public ResultScreen {
    std::optional<WaiwaiOverlay> overlay;
    std::array<std::unique_ptr<Chara3D>, 2> charas;
    int chara_motion = -1;
    bool reveal_done = false;

protected:
    double reveal_end_ms() override;
    bool run_failed() const override;

public:
    bool two_player_audio() const override { return true; }
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};

class WaiwaiRetryGameScreen : public RetryGameScreen {
protected:
    Screens select_screen() const override { return Screens::WAIWAI_SONG_SELECT; }
};
