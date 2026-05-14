#include "gauge.h"
#include "../../libs/texture.h"

ResultGauge::ResultGauge(GaugeMode mode, PlayerNum player_num, float gauge_length, bool is_2p)
    : mode(mode), player_num(player_num), gauge_length(gauge_length), is_2p(is_2p) {

    if (mode == GaugeMode::NORMAL) {
        gauge_max    = 87.0f;
        scale        = 10.0f / 11.0f;
        difficulty   = std::min(Difficulty::HARD, Difficulty(global_data.session_data[(int)player_num].selected_difficulty));
        clear_start  = {52, 60, 69};

        if (difficulty >= Difficulty::HARD)        string_diff = "_hard";
        else if (difficulty >= Difficulty::NORMAL)  string_diff = "_normal";
        else                                        string_diff = "_easy";

        gauge_fade_in        = (FadeAnimation*)tex.get_animation(17);
        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(20);
        gauge_fade_in->start();
        anims_loaded = true;

        if (gauge_length == gauge_max)
            state = ResultState::RAINBOW;
        else if (gauge_length >= clear_start[(int)difficulty] - 1)
            state = ResultState::CLEAR;
        else
            state = ResultState::FAIL;
    } else {
        // DAN mode: animations loaded lazily (same as DanResultGauge pattern)
        gauge_max     = 89.0f;
        scale         = 1.0f;
        clear_start   = {};
        string_diff   = "";
        visual_length = gauge_length * 8.0f;

        gauge_fade_in        = nullptr;
        tamashii_fire_change = nullptr;

        state = (gauge_length == gauge_max) ? ResultState::RAINBOW : ResultState::FAIL;
    }
}

void ResultGauge::update(double current_ms) {
    if (mode == GaugeMode::DAN && !anims_loaded) {
        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
        gauge_fade_in        = (FadeAnimation*)tex.get_animation(63);
        gauge_fade_in->start();
        anims_loaded = true;
    }

    if (state == ResultState::RAINBOW) {
        if (rainbow_start_ms < 0) rainbow_start_ms = current_ms;
        rainbow_frac = (float)fmod((current_ms - rainbow_start_ms) / 75.0, 8.0);
    }
    if (tamashii_fire_change) tamashii_fire_change->update(current_ms);
    if (gauge_fade_in) gauge_fade_in->update(current_ms);
}

void ResultGauge::draw(double external_fade) {
    if (!anims_loaded) return;

    // NORMAL mode uses its own internal fade; DAN mode uses the caller-supplied fade
    double base_fade    = (mode == GaugeMode::NORMAL) ? gauge_fade_in->attribute : external_fade;
    double rainbow_fade = std::min((double)gauge_fade_in->attribute, external_fade);

    std::string player_str = std::to_string(static_cast<int>(player_num)) + "p";

    if (mode == GaugeMode::NORMAL) {
        int gauge_length_int = (int)gauge_length;
        int clear_point      = clear_start[(int)difficulty];
        float bar_width      = tex.textures[tex.get_enum("gauge/" + player_str + "_bar")]->width;

        tex.draw_texture(tex.get_enum("gauge/" + (player_str + "_unfilled" + string_diff)),
                         {.scale = scale, .fade = base_fade, .index = is_2p});

        if (state == ResultState::RAINBOW) {
            int frame_a = (int)rainbow_frac % 8;
            int frame_b = (frame_a + 1) % 8;
            float t = rainbow_frac - (int)rainbow_frac;
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                             {.frame = frame_a, .scale = scale, .fade = base_fade, .index = is_2p});
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                             {.frame = frame_b, .scale = scale, .fade = (float)base_fade * t, .index = is_2p});
        } else {
            tex.draw_texture(tex.get_enum("gauge/" + (player_str + "_bar")),
                             {.scale = scale,
                              .x2 = std::min(gauge_length_int * bar_width, (clear_point - 1) * bar_width) - bar_width,
                              .fade = base_fade, .index = is_2p});
            if (gauge_length_int >= clear_point - 1)
                tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION,
                                 {.scale = scale, .x = (clear_point - 1) * bar_width,
                                  .fade = base_fade, .index = is_2p});
            if (gauge_length_int > clear_point) {
                tex.draw_texture(GAUGE::BAR_CLEAR_TOP,
                                 {.scale = scale, .x = clear_point * bar_width,
                                  .x2 = (gauge_length_int - clear_point) * bar_width,
                                  .fade = base_fade, .index = is_2p});
                tex.draw_texture(GAUGE::BAR_CLEAR_BOTTOM,
                                 {.scale = scale, .x = clear_point * bar_width,
                                  .x2 = (gauge_length_int - clear_point) * bar_width,
                                  .fade = base_fade, .index = is_2p});
            }
        }

        tex.draw_texture(tex.get_enum("gauge/overlay" + string_diff),
                         {.scale = scale, .fade = std::min(0.15, base_fade), .index = is_2p});
        tex.draw_texture(GAUGE::FOOTER, {.scale = scale, .fade = base_fade, .index = is_2p});

        if (gauge_length_int >= clear_point - 1) {
            tex.draw_texture(tex.get_enum("gauge/clear_" + global_data.config->general.language),
                             {.scale = scale, .fade = base_fade, .index = (int)difficulty + (is_2p * 3)});
            if (state == ResultState::RAINBOW) {
                tex.draw_texture(GAUGE::TAMASHII_FIRE,
                                 {.frame = (int)tamashii_fire_change->attribute, .scale = 0.75f * scale,
                                  .center = true, .fade = base_fade, .index = is_2p});
            }
            tex.draw_texture(GAUGE::TAMASHII, {.scale = scale, .fade = base_fade, .index = is_2p});
            int a = (int)tamashii_fire_change->attribute;
            if (state == ResultState::RAINBOW && (a == 0 || a == 1 || a == 4 || a == 5))
                tex.draw_texture(GAUGE::TAMASHII_OVERLAY,
                                 {.scale = scale, .fade = std::min(0.5, base_fade), .index = is_2p});
        } else {
            tex.draw_texture(tex.get_enum("gauge/clear_dark_" + global_data.config->general.language),
                             {.scale = scale, .fade = base_fade, .index = (int)difficulty + (is_2p * 3)});
            tex.draw_texture(GAUGE::TAMASHII_DARK, {.scale = scale, .fade = base_fade, .index = is_2p});
        }
    } else { // DAN mode
        // DAN mode
        tex.draw_texture(GAUGE::_1P_UNFILLED, {.fade = base_fade});

        if (state != ResultState::RAINBOW)
            tex.draw_texture(GAUGE::_1P_BAR, {.x2 = visual_length - 8, .fade = base_fade});

        if (state == ResultState::RAINBOW) {
            int frame_a = (int)rainbow_frac % 8;
            int frame_b = (frame_a + 1) % 8;
            float t = rainbow_frac - (int)rainbow_frac;
            tex.draw_texture(GAUGE::RAINBOW, {.frame = frame_a, .fade = (float)rainbow_fade});
            tex.draw_texture(GAUGE::RAINBOW, {.frame = frame_b, .fade = (float)rainbow_fade * t});
        }

        tex.draw_texture(GAUGE::OVERLAY, {.fade = std::min(base_fade, 0.15)});
        tex.draw_texture(GAUGE::FOOTER,  {.fade = base_fade});

        if (state == ResultState::RAINBOW) {
            tex.draw_texture(GAUGE::TAMASHII_FIRE,
                             {.frame = (int)tamashii_fire_change->attribute, .scale = 0.75f,
                              .center = true, .fade = base_fade});
            tex.draw_texture(GAUGE::TAMASHII, {.fade = base_fade});
            int f = (int)tamashii_fire_change->attribute;
            if (f == 0 || f == 1 || f == 4 || f == 5)
                tex.draw_texture(GAUGE::TAMASHII_OVERLAY, {.fade = std::min(base_fade, 0.5)});
        } else {
            tex.draw_texture(GAUGE::TAMASHII_DARK, {.fade = base_fade});
        }
    }
}
