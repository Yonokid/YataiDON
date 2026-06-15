#pragma once

#include "../libs/screen.h"
#include "../libs/video.h"
#include "../libs/text.h"
#include "../objects/title/warning_screen.h"
#include "../objects/title/attract_camera.h"
#include "../objects/title/camera_cloud.h"
#include "../objects/title/bana_advert.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/entry_overlay.h"

enum class TitleState {
    OP_VIDEO,
    WARNING,
    ATTRACT_VIDEO,
    ATTRACT_CAMERA
};

class TitleScreen : public Screen {
private:
    std::vector<fs::path> op_video_list;
    std::vector<fs::path> attract_video_list;

    TitleState state;

    std::optional<VideoPlayer> op_video;
    std::optional<VideoPlayer> attract_video;
    std::optional<WarningScreen> warning_board;
    std::optional<AttractCamera> attract_camera;
    std::optional<CameraCloud> camera_cloud;
    std::optional<BanaAdvertisement> bana_advert_1;
    std::optional<BanaAdvertisement> bana_advert_2;

    AllNetIcon allnet_indicator;
    CoinOverlay coin_overlay;
    EntryOverlay entry_overlay;

    std::unique_ptr<OutlinedText> hit_taiko_text;

    FadeAnimation* fade_out;
    FadeAnimation* text_overlay_fade;

    void scene_manager(double current_ms);
public:
    TitleScreen() : Screen("title") {
    }

    void on_screen_start() override;
    void load_videos();

    Screens on_screen_end(Screens next_screen) override;

    std::optional<Screens> update() override;

    void draw() override;
};
