#include "../libs/screen.h"
#include "../libs/text.h"
#include "../libs/texture.h"
#include "../libs/input.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/result/background.h"
#include "../objects/result/player.h"
#include "../objects/result/fade_in.h"

class ResultScreen : public Screen {
private:
    OutlinedText* song_info;
    FadeAnimation* fade_out;
    AllNetIcon allnet_indicator;
    std::optional<ResultBackground> background;
    double start_ms = 0;
    double skipped_time = 0;
    std::optional<ResultPlayer> player_1;
    std::optional<FadeIn> fade_in;

    void handle_input();

    void draw_overlay();

    void draw_song_info();
public:
    ResultScreen() : Screen("result") {
    }

    void on_screen_start() override;

    std::optional<Screens> update() override;

    Screens on_screen_end(Screens next_screen) override;

    void draw() override;
};
