#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/script.h"
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

class Background {
private:
    sol::table lua_object;

public:
    Background(PlayerNum player_num, float bpm, const std::string& scene_preset);
    ~Background();
    void update(double current_ms);
    void handle_good(PlayerNum player_num);
    void handle_ok(PlayerNum player_num);
    void handle_bad(PlayerNum player_num);
    void handle_drumroll(PlayerNum player_num);
    void handle_balloon(PlayerNum player_num);
    void draw_back();
    void draw_fore();
};
