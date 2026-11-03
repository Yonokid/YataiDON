#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "../libs/file/filesystem.h"
#include "../libs/gfx/text.h"
#include "../libs/screen.h"
#include "../libs/video/video.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/entry_overlay.h"
#include "../objects/global/kinotake_indicator.h"
#include "../objects/global/network_msg.h"
#include "../objects/title/attract_announce.h"
#include "../objects/title/attract_scene.h"
#include "../objects/title/tournament_attract.h"
#include "../objects/title/warning_screen.h"

enum class TitleState {
    OP_VIDEO,
    WARNING,
    ATTRACT_VIDEO,
    ATTRACT_CAMERA,
    TOURNAMENT
};

class TitleScreen : public Screen {
private:
    std::vector<fs::path> op_video_list;
    std::vector<fs::path> attract_video_list;

    TitleState state = TitleState::OP_VIDEO;

    std::optional<VideoPlayer> op_video;
    std::optional<VideoPlayer> attract_video;
    std::optional<WarningScreen> warning_board;
    std::optional<AttractScene> attract_scene;
    std::optional<TournamentAttract> tournament;
    TextureObject* t_caution = nullptr;

    AllNetIcon allnet_indicator;
    KinotakeIndicator kinotake_indicator;
    bool kinotake_on = false;
    void update_announce(double current_ms);
    AttractAnnounce announce;
    std::string announce_mode;
    CoinOverlay coin_overlay;
    EntryOverlay entry_overlay;
    NetworkMsg network_msg;

    bool gacha_busy = false;
    bool gacha_waiting = false;
    std::optional<int> gacha_puchichara;
    double msg_until_ms = 0.0;

    std::unique_ptr<OutlinedText> hit_taiko_text;

    FadeAnimation* fade_out = nullptr;
    FadeAnimation* text_overlay_fade = nullptr;

    void scene_manager(double current_ms);
    void reset_attract_objects();

    bool update_gacha(double current_ms);
    void start_entry();

public:
    TitleScreen() : Screen("title") {}

    void on_screen_start() override;
    void load_videos();

    Screens on_screen_end(Screens next_screen) override;

    std::optional<Screens> update() override;

    void draw() override;
};
