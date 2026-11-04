#pragma once

#include <future>

#include "../libs/playback_clock.h"
#include "../libs/screen.h"
#include "../libs/video/video.h"
#include "../objects/game/player.h"
#include "../objects/game/result_transition.h"
#include "../objects/game/song_info.h"
#include "../objects/game/transition.h"
#include "../objects/global/allnet_indicator.h"

class GameScreen : public Screen {
protected:
    GameScreen(const std::string& name) : Screen(name) {}

    PlaybackClock clock;

    void update_gameplay(double current_ms);
    virtual void create_players();
    virtual float song_playback_rate() const { return 1.0f; }
    enum class AudioLoadMode {
        Sync,
        Async
    };

    enum class BackgroundNotification {
        Enabled,
        Disabled
    };

    void end_song(double end_time, double transition_delay, BackgroundNotification notification);
    void initialize_song_movie();
    void set_session_song_info(PlayerNum player_num);
    void load_song_audio(AudioLoadMode mode);
    double audio_start_ms() const;
    void reset_playback();
    void reset_song_clock(double opening_delay_ms = 0.0);
    void stop_song_audio();
    void unload_song();
    virtual void release_chart_resources() {}
    void pause_playback();
    void resume_playback(std::optional<double> chart_ms = std::nullopt);
    virtual PlayerNum restart_player_num() const { return global_data.player_num; }

public:
    GameScreen() : Screen("game") {}

    double start_delay = 1000.0;
    bool song_started = false;
    bool score_saved = false;
    float bpm = 0.0f;

    std::optional<VideoPlayer> movie;
    std::optional<std::string> song_music;
    std::optional<SongParser> parser;
    std::string scene_preset;
    std::vector<std::unique_ptr<Player>> players;
    SongInfo song_info;
    std::optional<Transition> transition;
    ResultTransition result_transition;
    static constexpr double kFastResultTransitionDelayMs = 3000.0;
    AllNetIcon allnet_indicator;
    std::optional<Background> background;

    void on_screen_start() override;

    virtual PlayerNum scene_player_num() const { return global_data.player_num; }
    virtual std::string background_scene_preset() const { return scene_preset; }

    virtual Modifiers get_player_modifiers(PlayerNum pn);

    Screens on_screen_end(Screens next_screen) override;

    void load_hitsounds();

    virtual void initialize_chart(const fs::path& song);

    void start_song(double chart_ms);

    void poll_pending_song();
    void set_players_audio_end();

    void restart_song();

    void toggle_pause();

    void resync_song(double current_ms);

    static constexpr int SKIP_HITS = 10;
    PlayerNum skip_lane = PlayerNum::ALL;
    int skip_count = 0;
    int skip_last = -1;
    size_t skip_play_hits = 0;
    bool skipped = false;
    std::unique_ptr<OutlinedText> skip_text;
    void init_skip();
    void update_skip();
    void poll_skip();
    void push_skip_state();
    void do_skip();
    void draw_skip();

    std::optional<Screens> global_keys();

    void update_background(double current_ms);

    void save_score(const std::string& player_id, PlayerNum player_num);

    std::optional<Screens> update() override;

    virtual void draw_players();

    void draw_overlay(bool with_song_info = true);

    void draw() override;

private:
    void play_song_audio(double position_seconds);

    float paused_audio_time = 0.0f;
    std::future<std::string> pending_song_load;
};
