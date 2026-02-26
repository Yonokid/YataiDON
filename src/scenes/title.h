#pragma once

#include "../libs/screen.h"
#include "../libs/video.h"
#include "../libs/text.h"
#include "../libs/texture.h"
#include "../input.h"
#include "../audio.h"
#include "../objects/global/allnet_indicator.h"

#include <random>

#include "../objects/title/warning_screen.h"

enum TitleState {
    OP_VIDEO,
    WARNING,
    ATTRACT_VIDEO,
};

class TitleScreen : public Screen {
private:
    std::vector<fs::path> op_video_list;
    std::vector<fs::path> attract_video_list;

    TitleState state;

    std::optional<VideoPlayer> op_video;
    std::optional<VideoPlayer> attract_video;
    std::optional<WarningScreen> warning_board;

    AllNetIcon allnet_indicator;

    OutlinedText* hit_taiko_text;

    FadeAnimation* fade_out;
    FadeAnimation* text_overlay_fade;

    void scene_manager(double current_ms);
public:
    TitleScreen() : Screen("title") {
    }

    void on_screen_start() override;

    Screens on_screen_end(Screens next_screen) override;

    std::optional<Screens> update() override;

    void draw() override;
};
