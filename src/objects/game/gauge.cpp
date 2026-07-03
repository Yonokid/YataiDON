#include "gauge.h"
#include "../../libs/texture.h"

Gauge::Gauge(GaugeMode mode, PlayerNum player_num, int total_notes, int difficulty, int level)
    : mode(mode), player_num(player_num), total_notes(total_notes),
      gauge_length(0), previous_length(0), is_clear(false), is_rainbow(false),
      tamashii_fire_change(nullptr), gauge_update_anim(nullptr) {

    if (mode == GaugeMode::NORMAL) {
        gauge_max = 87.0f;
        this->difficulty = std::min((int)Difficulty::ONI, difficulty);
        this->level = std::min(10, level);

        clear_start = {52, 60, 69, 69};

        if (this->difficulty == (int)Difficulty::HARD) {
            string_diff = "_hard";
        } else if (this->difficulty == (int)Difficulty::NORMAL) {
            string_diff = "_normal";
        } else if (this->difficulty == (int)Difficulty::EASY) {
            string_diff = "_easy";
        } else {
            string_diff = "_hard";
        }

        table = {
            {
                {36.0f, 0.75f, -0.5f},
                {38.0f, 0.75f, -0.5f},
                {38.0f, 0.75f, -0.5f},
                {44.0f, 0.75f, -0.5f},
                {44.0f, 0.75f, -0.5f},
            },
            {
                {45.939f, 0.75f, -0.5f},
                {45.939f, 0.75f, -0.5f},
                {48.676f, 0.75f, -0.5f},
                {49.232f, 0.75f, -0.75f},
                {52.5f, 0.75f, -1.0f},
                {52.5f, 0.75f, -1.0f},
                {52.5f, 0.75f, -1.0f},
            },
            {
                {54.325f, 0.75f, -0.75f},
                {54.325f, 0.75f, -0.75f},
                {50.774f, 0.75f, -1.0f},
                {48.410f, 0.75f, -1.17f},
                {47.246f, 0.75f, -1.25f},
                {48.120f, 0.75f, -1.25f},
                {48.120f, 0.75f, -1.25f},
                {48.120f, 0.75f, -1.25f},
            },
            {
                {56.603f, 0.5f, -1.6f},
                {56.603f, 0.5f, -1.6f},
                {56.603f, 0.5f, -1.6f},
                {56.603f, 0.5f, -1.6f},
                {56.603f, 0.5f, -1.6f},
                {56.603f, 0.5f, -1.6f},
                {56.603f, 0.5f, -1.6f},
                {56.0f, 0.5f, -2.0f},
                {61.428f, 0.5f, -2.0f},
                {61.428f, 0.5f, -2.0f},
            }
        };

        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
        gauge_update_anim    = (FadeAnimation*)tex.get_animation(10);
    } else {
        // DAN: animations loaded lazily in update() since this object may be
        // constructed before the texture system is ready (class-member initializer)
        gauge_max = 89.0f;
        this->difficulty = 0;
        this->level      = 1;
    }
}

void Gauge::add_good() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_length = (int)gauge_length;

    if (mode == GaugeMode::NORMAL) {
        gauge_length += (1.0f / total_notes) *
                        (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
    } else {
        gauge_length  += (1.0f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
        visual_length  = gauge_length * tex.textures[GAUGE_DAN::_1P_BAR]->width;
    }
    if (gauge_length > gauge_max) gauge_length = gauge_max;
}

void Gauge::add_ok() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_length = (int)gauge_length;

    if (mode == GaugeMode::NORMAL) {
        gauge_length += ((1.0f * table[difficulty][level - 1].ok_multiplier) / total_notes) *
                        (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
    } else {
        gauge_length  += (0.5f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
        visual_length  = gauge_length * tex.textures[GAUGE_DAN::_1P_BAR]->width;
    }
    if (gauge_length > gauge_max) gauge_length = gauge_max;
}

void Gauge::add_bad() {
    previous_length = (int)gauge_length;

    if (mode == GaugeMode::NORMAL) {
        gauge_length += ((1.0f * table[difficulty][level - 1].bad_multiplier) / total_notes) *
                        (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
        if (gauge_length < 0) gauge_length = 0;
        if (previous_length == gauge_max && gauge_length < gauge_max) {
            if (rainbow_fade_in.has_value()) rainbow_fade_in.reset();
            rainbow_start_ms = -1.0;
            rainbow_frac     = 0.0f;
        }
    } else {
        gauge_length  -= (2.0f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
        if (gauge_length < 0) gauge_length = 0;
        visual_length  = gauge_length * tex.textures[GAUGE_DAN::_1P_BAR]->width;
    }
}

void Gauge::update(double current_ms) {
    if (mode == GaugeMode::DAN && !anims_loaded) {
        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
        gauge_update_anim    = (FadeAnimation*)tex.get_animation(10);
        anims_loaded = true;
    }

    is_rainbow = (gauge_length == gauge_max);
    is_clear   = (mode == GaugeMode::NORMAL)
                 ? gauge_length > clear_start[std::min(difficulty, (int)Difficulty::HARD)] - 1
                 : is_rainbow;

    if (gauge_length == gauge_max && !rainbow_fade_in.has_value()) {
        rainbow_fade_in = (FadeAnimation*)tex.get_animation(63);
        rainbow_fade_in.value()->start();
        rainbow_start_ms = current_ms;
    }

    if (gauge_update_anim)    gauge_update_anim->update(current_ms);
    if (tamashii_fire_change) tamashii_fire_change->update(current_ms);

    if (rainbow_fade_in.has_value()) {
        rainbow_fade_in.value()->update(current_ms);
        rainbow_frac = (float)fmod((current_ms - rainbow_start_ms) / 75.0, 8.0);
    }
}

void Gauge::draw(float y) {
    if (mode == GaugeMode::NORMAL) {
        bool mirrored = y > tex.screen_height / 2.0f;
        Mirror mirror = mirrored ? Mirror::VERTICAL : Mirror::NONE;

        tex.draw_texture(tex.get_enum("gauge/border" + string_diff), {.mirror = mirror, .y = y});

        tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_unfilled" + string_diff)),
                         {.mirror = mirror, .y = y, .index = mirrored});

        int gauge_length_int = (int)gauge_length;
        int clear_point      = clear_start[difficulty];
        float bar_width      = tex.textures[tex.get_enum("gauge/" + std::to_string((int)player_num) + "p_bar")]->width;

        tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_bar")),
                         {.y = y, .x2 = std::min(gauge_length_int * bar_width, (clear_point - 1) * bar_width) - bar_width, .index = mirrored});

        if (gauge_length_int >= clear_point - 1)
            tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION,
                             {.mirror = mirror, .x = (clear_point - 1) * bar_width, .y = y, .index = mirrored});

        if (gauge_length_int > clear_point) {
            tex.draw_texture(GAUGE::BAR_CLEAR_TOP,
                             {.mirror = mirror, .x = clear_point * bar_width, .y = y,
                              .x2 = (gauge_length_int - clear_point) * bar_width, .index = mirrored});
            tex.draw_texture(GAUGE::BAR_CLEAR_BOTTOM,
                             {.x = clear_point * bar_width, .y = y,
                              .x2 = (gauge_length_int - clear_point) * bar_width, .index = mirrored});
        }

        if (gauge_length_int == gauge_max && rainbow_fade_in.has_value()) {
            float fade    = rainbow_fade_in.value()->attribute;
            int   frame_a = (int)rainbow_frac % 8;
            int   frame_b = (frame_a + 1) % 8;
            float t       = rainbow_frac - (int)rainbow_frac;
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                             {.frame = frame_a, .mirror = mirror, .y = y, .fade = fade, .index = mirrored});
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                             {.frame = frame_b, .mirror = mirror, .y = y, .fade = fade * t, .index = mirrored});
        }

        if (gauge_length_int <= gauge_max && gauge_length_int > previous_length) {
            if (gauge_length_int == clear_start[difficulty]) {
                tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION_FADE,
                                 {.mirror = mirror, .x = gauge_length_int * bar_width, .y = y,
                                  .fade = gauge_update_anim->attribute, .index = mirrored});
            } else if (gauge_length_int > clear_start[difficulty]) {
                tex.draw_texture(GAUGE::BAR_CLEAR_FADE,
                                 {.x = gauge_length_int * bar_width, .y = y,
                                  .fade = gauge_update_anim->attribute, .index = mirrored});
            } else {
                tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_bar_fade")),
                                 {.x = gauge_length_int * bar_width, .y = y,
                                  .fade = gauge_update_anim->attribute, .index = mirrored});
            }
        }

        tex.draw_texture(tex.get_enum("gauge/overlay" + string_diff),
                         {.mirror = mirror, .y = y, .fade = 0.15f, .index = mirrored});

        if (gauge_length_int >= clear_point - 1) {
            tex.draw_texture(tex.get_enum("gauge/clear_" + global_data.config->general.language),
                             {.y = y, .index = std::min(2, difficulty) + (mirrored * 3)});
            if (is_rainbow) {
                tex.draw_texture(GAUGE::TAMASHII_FIRE,
                                 {.frame = (int)tamashii_fire_change->attribute, .scale = 0.75f,
                                  .center = true, .y = y, .index = mirrored});
            }
            tex.draw_texture(GAUGE::TAMASHII, {.y = y, .index = mirrored});
            int fire_frame = (int)tamashii_fire_change->attribute;
            if (is_rainbow && (fire_frame == 0 || fire_frame == 1 || fire_frame == 4 || fire_frame == 5))
                tex.draw_texture(GAUGE::TAMASHII_OVERLAY, {.y = y, .fade = 0.5f, .index = mirrored});
        } else {
            tex.draw_texture(tex.get_enum("gauge/clear_dark_" + global_data.config->general.language),
                             {.y = y, .index = std::min(2, difficulty) + (mirrored * 3)});
            tex.draw_texture(GAUGE::TAMASHII_DARK, {.y = y, .index = mirrored});
        }
    } else {
        // DAN mode
        if (!anims_loaded) return;

        tex.draw_texture(GAUGE_DAN::BORDER,      {});
        tex.draw_texture(GAUGE_DAN::_1P_UNFILLED, {});
        tex.draw_texture(GAUGE_DAN::_1P_BAR,     {.x2 = visual_length - 8});

        if (is_rainbow && rainbow_fade_in.has_value()) {
            float fade    = rainbow_fade_in.value()->attribute;
            int   frame_a = (int)rainbow_frac % 8;
            int   frame_b = (frame_a + 1) % 8;
            float t       = rainbow_frac - (int)rainbow_frac;
            tex.draw_texture(GAUGE_DAN::RAINBOW, {.frame = frame_a, .fade = fade});
            tex.draw_texture(GAUGE_DAN::RAINBOW, {.frame = frame_b, .fade = fade * t});
        }

        if ((int)gauge_length <= (int)gauge_max && (int)gauge_length > (int)previous_length)
            tex.draw_texture(GAUGE_DAN::_1P_BAR_FADE,
                             {.x = visual_length - 8, .fade = gauge_update_anim->attribute});

        tex.draw_texture(GAUGE_DAN::OVERLAY, {.fade = 0.15f});

        if (is_rainbow) {
            tex.draw_texture(GAUGE_DAN::TAMASHII_FIRE,
                             {.frame = (int)tamashii_fire_change->attribute, .scale = 0.75f, .center = true});
            tex.draw_texture(GAUGE_DAN::TAMASHII, {});
            int f = (int)tamashii_fire_change->attribute;
            if (f == 0 || f == 1 || f == 4 || f == 5)
                tex.draw_texture(GAUGE_DAN::TAMASHII_OVERLAY, {.fade = 0.5f});
        } else {
            tex.draw_texture(GAUGE_DAN::TAMASHII_DARK, {});
        }
    }
}
