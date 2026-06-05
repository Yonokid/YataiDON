#pragma once

#include "../libs/screen.h"
#include "../libs/video.h"
#include "../objects/game/player.h"
#include "../objects/game/transition.h"
#include "../objects/game/song_info.h"
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
    double ms_from_start;
    double start_delay;
    bool song_started;
    bool paused;
    bool score_saved;
    int pause_time;
    float bpm;

    std::optional<VideoPlayer> movie;
    std::optional<std::string> song_music;
    std::optional<SongParser> parser;
    std::string scene_preset;
    std::vector<std::unique_ptr<Player>> players;
    SongInfo song_info;
    std::optional<Transition> transition;
    ResultTransition result_transition;
    AllNetIcon allnet_indicator;
    std::optional<Background> background;

    void on_screen_start() override;

    virtual Modifiers get_player_modifiers(PlayerNum pn);

    Screens on_screen_end(Screens next_screen) override;

    void load_hitsounds();

    virtual void init_tja(fs::path song);

    void start_song(double ms_from_start);

    void restart_song();

    void pause_song();

    void resync_song(double ms_from_start);

    void end_song();

    std::optional<Screens> global_keys();

    void update_background(double current_ms);

    void save_score(int player_id, PlayerNum player_num);

    std::optional<Screens> update() override;

    void draw_players();

    void draw_overlay();

    void draw() override;
};
