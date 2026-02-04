#pragma once

#include "../libs/screen.h"
#include "../libs/utils.h"
#include "../libs/parsers/tja.h"
#include "../libs/global_data.h"
#include "../libs/animation.h"

#include "../objects/game/player.h"

class GameScreen : public Screen {
private:
    ray::Shader mask_shader;
    volatile double start_ms;
    volatile double current_ms;
    volatile double end_ms;
    double start_delay;
    bool song_started;
    bool paused;
    int pause_time;
    float bpm;
    //int audio_time;
    //int last_resync;
    //std::optional<VideoPlayer> movie;
    std::optional<std::string> song_music;
    std::optional<TJAParser> parser;
    std::string scene_preset;
    std::optional<Player> player_1;


public:
    GameScreen() : Screen("game") {
    }

    void on_screen_start() override;

    std::string on_screen_end(const std::string& next_screen) override;

    void load_hitsounds();

    void init_tja(fs::path song);

    void start_song(double ms_from_start);

    void update_audio(double ms_from_start);

    std::nullopt_t global_keys();

    std::optional<std::string> update() override;

    void draw_overlay();

    void draw() override;
};
