#pragma once
#include "../../libs/file/script.h"

sol::protected_function load_skin_module_fn(const char* module, const char* fn_name);
sol::protected_function cached_skin_module_fn(const char* module, const char* fn_name, const char* global_name);

class MoveAnimation;
float option_panel_offset(const MoveAnimation* move, bool confirmed);

class BaseBox;
class SongSelectPlayer;
class Navigator;
class DiffSortSelect;
class GenreBG;

class SongSelectScript : public LuaScript {
private:
    double clock_offset_ms = 0.0;
    std::optional<double> fast_open_started_ms;

    double animation_ms();

    template<typename... Args>
    void call(sol::protected_function& fn, std::string_view context, Args&&... args) {
        struct ClockScope {
            std::optional<double> previous = script_manager.current_ms_override;

            ~ClockScope() { script_manager.current_ms_override = previous; }
        } scope;

        script_manager.current_ms_override = animation_ms();

        LuaScript::call(fn, context, std::forward<Args>(args)...);
    }

    // Like call(), for a hook that returns true/false/nothing. Only an explicit `false` declines.
    template<typename... Args>
    bool call_handled(sol::protected_function& fn, std::string_view context, Args&&... args) {
        struct ClockScope {
            std::optional<double> previous = script_manager.current_ms_override;

            ~ClockScope() { script_manager.current_ms_override = previous; }
        } scope;

        script_manager.current_ms_override = animation_ms();

        auto r = call_r<sol::object>(fn, context, std::forward<Args>(args)...);
        if (!r) {
            return true; // a Lua error already logged: do not draw a second time
        }
        return !(r->template is<bool>() && !r->template as<bool>());
    }

    // module from the skin chain that supplies a hook the skin's SongSelect does not define
    sol::protected_function load_default_hook(const char* module, const char* fn_name);

    sol::protected_function fn_update;
    sol::protected_function fn_restart_text_fade;
    sol::protected_function fn_draw_footer;
    sol::protected_function fn_draw_overlays;
    sol::protected_function fn_draw_top;
    sol::protected_function fn_draw_box;
    sol::protected_function fn_draw_box_bg;
    sol::protected_function fn_draw_background;
    sol::protected_function fn_draw_genre_bg;
    sol::protected_function fn_draw_selector;
    sol::protected_function fn_draw_option_panel;
    sol::protected_function fn_draw_sort_window;
    sol::protected_function fn_default_option_panel;
    sol::protected_function fn_default_sort_window;
    sol::protected_function fn_default_selector;
    sol::protected_function fn_default_background_diffs;
    sol::protected_function fn_draw_search_box;
    sol::protected_function fn_default_search_box;
    sol::protected_function fn_draw_background_diffs;
    sol::protected_function fn_draw_song_num;

    sol::object box_to_lua(BaseBox* box);

public:
    SongSelectScript();
    void start_folder_open();
    void finish_folder_open();
    void update(double current_ms);
    void restart_text_fade();
    void draw_footer();
    void draw_overlays(int state);
    void draw_top(float dan_progress);

    bool draw_box(BaseBox* box);
    bool has_draw_box() const { return fn_draw_box.valid(); } // the skin draws the wheel itself
    bool draw_box_bg(BaseBox* box);
    bool has_box_bg() const { return fn_draw_box_bg.valid(); }
    bool draw_background(Navigator* nav);
    bool draw_genre_bg(GenreBG* genre_bg, float start_pos, float end_pos, BaseBox* folder);
    // true when the skin's own hook drew it (the default player_ui module draws it otherwise)
    bool draw_selector(SongSelectPlayer* player, bool is_half, float fade_in, int pass);
    bool draw_background_diffs(SongSelectPlayer* player);
    // the search overlay: search text and the open animation (bg_resize, fade_in)
    bool draw_search_box(const std::string& search, double bg_resize, double fade_in);
    // true when the skin's own hook drew the panel (the default module draws it otherwise)
    bool draw_option_panel(SongSelectPlayer* player, int kind);

    bool has_sort_window() const { return fn_draw_sort_window.valid(); }
    bool draw_sort_window(DiffSortSelect* window);
    bool draw_song_num();
};
