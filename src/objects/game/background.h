#include "../../libs/global_data.h"
#include <sol/sol.hpp>

class Background {
private:
    sol::table lua_object;
    sol::protected_function fn_update;
    sol::protected_function fn_handle_good;
    sol::protected_function fn_handle_ok;
    sol::protected_function fn_handle_bad;
    sol::protected_function fn_handle_drumroll;
    sol::protected_function fn_handle_balloon;
    sol::protected_function fn_handle_gauge;
    sol::protected_function fn_draw_back;
    sol::protected_function fn_draw_fore;

public:
    Background(PlayerNum player_num, float bpm, const std::string& scene_preset);
    ~Background();
    void update(double current_ms, float bpm);
    void handle_good(PlayerNum player_num);
    void handle_ok(PlayerNum player_num);
    void handle_bad(PlayerNum player_num);
    void handle_drumroll(PlayerNum player_num);
    void handle_balloon(PlayerNum player_num);
    void handle_gauge(PlayerNum player_num, float progress, bool is_clear, bool is_rainbow);
    void draw_back();
    void draw_fore();
};
