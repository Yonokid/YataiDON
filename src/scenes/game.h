#pragma once

#include "../libs/screen.h"
#include "../libs/parsers/tja.h"
#include "../libs/video.h"
#include "../libs/scores.h"

#include "../objects/game/player.h"
#include "../objects/game/song_info.h"
#include "../objects/game/transition.h"
#include "../objects/game/result_transition.h"
#include "../objects/global/allnet_indicator.h"

class GameScreen : public Screen {
protected:
    GameScreen(const std::string& name) : Screen(name) {}

public:
    GameScreen() : Screen("game") {
    }

    ray::Shader mask_shader;
    double start_ms;
    double current_ms;
    double end_ms;
    double start_delay;
    bool song_started;
    bool paused;
    bool score_saved;
    int pause_time;
    float bpm;

    std::optional<VideoPlayer> movie;
    std::optional<std::string> song_music;
    std::optional<TJAParser> parser;
    std::string scene_preset;
    std::vector<std::unique_ptr<Player>> players;
    SongInfo song_info;
    std::optional<Transition> transition;
    ResultTransition result_transition;
    AllNetIcon allnet_indicator;
    std::optional<Background> background;

    void on_screen_start() override;

    Screens on_screen_end(Screens next_screen) override;

    void load_hitsounds();

    virtual void init_tja(fs::path song);

    void start_song(double ms_from_start);

    void pause_song();

    std::optional<Screens> global_keys();

    void update_background(double current_time);

    std::optional<Screens> update() override;

    void draw_players();

    void draw_overlay();

    void draw() override;
};
