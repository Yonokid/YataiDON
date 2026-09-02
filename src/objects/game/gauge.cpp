#include "gauge.h"
#include "../../libs/texture.h"
#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>

constexpr double kRainbowCelMs = 1000.0 / 60.0 * 5.0;
constexpr float kOverlayAlpha = 77.0f / 256.0f;
namespace {

bool r48_disabled() {
    return true;
}

struct ChnGaugeRow { float clear_rate; float loss_mult; };

const ChnGaugeRow CHN_EASY[5] = {
    {36.0f, 0.5f}, {38.0f, 0.5f}, {38.0f, 0.5f}, {44.0f, 0.5f}, {44.0f, 0.5f}};
const ChnGaugeRow CHN_NORMAL[7] = {
    {45.936f, 0.5f}, {45.936f, 0.5f}, {48.669f, 0.5f}, {49.219f, 0.75f},
    {52.5f, 1.0f}, {52.5f, 1.0f}, {52.5f, 1.0f}};
const ChnGaugeRow CHN_HARD[8] = {
    {54.28f, 0.75f}, {54.25f, 0.75f}, {50.71f, 1.0f}, {48.415f, 7.0f/6.0f},
    {47.26f, 1.25f}, {48.12f, 1.25f}, {48.12f, 1.25f}, {48.12f, 1.25f}};
const ChnGaugeRow CHN_ONI[10] = {
    {56.603f, 1.6f}, {56.603f, 1.6f}, {56.603f, 1.6f}, {56.603f, 1.6f},
    {56.603f, 1.6f}, {56.603f, 1.6f}, {56.603f, 1.6f},
    {56.0f, 2.0f}, {61.0f, 2.0f}, {61.0f, 2.0f}};

const int CHN_NORMA[4] = {6000, 7000, 7000, 8000};
const float CHN_OKM[4] = {0.75f, 0.75f, 0.75f, 0.5f};
const int CHN_ART[4] = {0, 1, 1, 2};

int chn_round(double x) {
    return (int)std::floor(x + 0.5);
}

double chn_delta(int point) {
    return (double)((float)((float)(65536.0 * (double)point) * 0.000015258789f));
}

} // namespace

Gauge::Gauge()
    : gauge_length(0), gauge_max(87.0f), mode(GaugeMode::NORMAL),
      player_num(PlayerNum::P1), total_notes(0), difficulty(0), level(1),
      previous_length(0), is_clear(false), is_rainbow(false),
      tamashii_fire_change(nullptr), gauge_update_anim(nullptr) {}

Gauge::Gauge(GaugeMode mode, PlayerNum player_num, int total_notes, int difficulty, int level)
    : gauge_length(0), mode(mode), player_num(player_num), total_notes(total_notes),
      previous_length(0), is_clear(false), is_rainbow(false),
      tamashii_fire_change(nullptr), gauge_update_anim(nullptr) {

    if (mode == GaugeMode::NORMAL) {
        gauge_max = 87.0f;
        this->difficulty = std::min((int)Difficulty::ONI, difficulty);
        this->level = std::min(10, level);

        clear_start = {52, 60, 69, 69};

        chn_model = !r48_disabled();
        if (chn_model) {
            int d = this->difficulty;
            if (d < 0) d = 0;
            const ChnGaugeRow* rows;
            int nrows;
            switch (d) {
                case (int)Difficulty::EASY:   rows = CHN_EASY;   nrows = 5;  break;
                case (int)Difficulty::NORMAL: rows = CHN_NORMAL; nrows = 7;  break;
                case (int)Difficulty::HARD:   rows = CHN_HARD;   nrows = 8;  break;
                default:                      rows = CHN_ONI;    nrows = 10; break;
            }
            int lvl = this->level;
            if (lvl < 1) lvl = 1;
            if (lvl > nrows) lvl = nrows;
            const ChnGaugeRow& row = rows[lvl - 1];
            norma     = CHN_NORMA[d];
            art_index = CHN_ART[d];
            soul      = 0.0;
            if (total_notes > 0) {
                double raw = (double)norma / ((double)total_notes * row.clear_rate / 100.0);
                tp_great = chn_round(raw);
                tp_good  = chn_round(raw * CHN_OKM[d]);
                tp_loss  = -chn_round(raw * row.loss_mult);
            } else {
                tp_great = tp_good = tp_loss = 0;
            }
            string_diff = (art_index == 0) ? "_easy"
                        : (art_index == 1) ? "_normal" : "_hard";
        } else {
            art_index = std::min(2, this->difficulty);
            if (this->difficulty == (int)Difficulty::HARD) {
                string_diff = "_hard";
            } else if (this->difficulty == (int)Difficulty::NORMAL) {
                string_diff = "_normal";
            } else if (this->difficulty == (int)Difficulty::EASY) {
                string_diff = "_easy";
            } else {
                string_diff = "_hard";
            }
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

Gauge Gauge::make_result(GaugeMode mode, PlayerNum player_num, float gauge_length, bool is_2p) {
    Gauge g;
    g.is_result    = true;
    g.mode         = mode;
    g.player_num   = player_num;
    g.gauge_length = gauge_length;
    g.is_2p        = is_2p;

    if (mode == GaugeMode::NORMAL) {
        g.gauge_max    = 87.0f;
        g.result_scale = 10.0f / 11.0f;
        int seldiff    = global_data.session_data[(int)player_num].selected_difficulty;
        g.difficulty   = std::min((int)Difficulty::HARD, seldiff);
        g.clear_start  = {52, 60, 69};

        g.chn_model = !r48_disabled();
        if (g.chn_model) {
            int d = seldiff;
            if (d < 0) d = 0;
            if (d > (int)Difficulty::ONI) d = (int)Difficulty::ONI;
            g.norma     = CHN_NORMA[d];
            g.art_index = CHN_ART[d];
            g.string_diff = (g.art_index == 0) ? "_easy"
                          : (g.art_index == 1) ? "_normal" : "_hard";
            g.soul = (double)gauge_length / 87.0 * 10000.0;
        } else {
            g.art_index = g.difficulty;
            if (g.difficulty >= (int)Difficulty::HARD)        g.string_diff = "_hard";
            else if (g.difficulty >= (int)Difficulty::NORMAL) g.string_diff = "_normal";
            else                                              g.string_diff = "_easy";
        }

        g.gauge_fade_in        = (FadeAnimation*)tex.get_animation(tex.has_animation(70) ? 70 : 17);
        g.tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(tex.has_animation(71) ? 71 : 20);
        g.gauge_fade_in->start();
        g.anims_loaded = true;

        if (gauge_length == g.gauge_max)
            g.state = ResultState::RAINBOW;
        else if (g.chn_model
                 ? (g.soul >= (double)g.norma - 0.5)   // -0.5: float round-trip guard
                 : (gauge_length >= g.clear_start[g.difficulty] - 1))
            g.state = ResultState::CLEAR;
        else
            g.state = ResultState::FAIL;
    } else {
        // DAN mode: animations loaded lazily in update()
        g.gauge_max = 89.0f;
        g.state = (gauge_length == g.gauge_max) ? ResultState::RAINBOW : ResultState::FAIL;
    }
    return g;
}

void Gauge::add_good() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_length = (int)gauge_length;

    if (mode == GaugeMode::NORMAL) {
        if (chn_model) {
            soul += chn_delta(tp_great);
            if (soul > 10000.0) soul = 10000.0;
            if (soul < 0.0) soul = 0.0;
            gauge_length = (float)(soul / 10000.0 * gauge_max);
            if (std::getenv("YATAIDON_R33_GLSTATE"))
                spdlog::info("[r48gauge] good +{} soul={:.1f}", tp_great, soul);
        } else {
            gauge_length += (1.0f / total_notes) *
                            (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
        }
    } else {
        gauge_length += (1.0f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
    }
    if (gauge_length > gauge_max) gauge_length = gauge_max;
}

void Gauge::add_ok() {
    if (gauge_update_anim) gauge_update_anim->start();
    previous_length = (int)gauge_length;

    if (mode == GaugeMode::NORMAL) {
        if (chn_model) {
            soul += chn_delta(tp_good);
            if (soul > 10000.0) soul = 10000.0;
            if (soul < 0.0) soul = 0.0;
            gauge_length = (float)(soul / 10000.0 * gauge_max);
            if (std::getenv("YATAIDON_R33_GLSTATE"))
                spdlog::info("[r48gauge] ok +{} soul={:.1f}", tp_good, soul);
        } else {
            gauge_length += ((1.0f * table[difficulty][level - 1].ok_multiplier) / total_notes) *
                            (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
        }
    } else {
        gauge_length += (0.5f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
    }
    if (gauge_length > gauge_max) gauge_length = gauge_max;
}

void Gauge::add_bad() {
    previous_length = (int)gauge_length;

    if (mode == GaugeMode::NORMAL) {
        if (chn_model) {
            soul += chn_delta(tp_loss);
            if (soul > 10000.0) soul = 10000.0;
            if (soul < 0.0) soul = 0.0;
            gauge_length = (float)(soul / 10000.0 * gauge_max);
            if (std::getenv("YATAIDON_R33_GLSTATE"))
                spdlog::info("[r48gauge] bad {} soul={:.1f}", tp_loss, soul);
        } else {
            gauge_length += ((1.0f * table[difficulty][level - 1].bad_multiplier) / total_notes) *
                            (100.0f * (clear_start[difficulty] / table[difficulty][level - 1].clear_rate));
            if (gauge_length < 0) gauge_length = 0;
        }
        if (previous_length == gauge_max && gauge_length < gauge_max) {
            if (rainbow_fade_in.has_value()) rainbow_fade_in.reset();
            rainbow_start_ms = -1.0;
            rainbow_frac     = 0.0f;
        }
    } else {
        gauge_length -= (2.0f / (total_notes * (gauge_max / 100.0f))) * 100.0f;
        if (gauge_length < 0) gauge_length = 0;
    }
}

void Gauge::update(double current_ms) {
    if (is_result) {
        if (mode == GaugeMode::DAN && !anims_loaded) {
            tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
            if (tex.has_animation(63))      gauge_fade_in = (FadeAnimation*)tex.get_animation(63);
            else if (tex.has_animation(10)) gauge_fade_in = (FadeAnimation*)tex.get_animation(10);
            if (gauge_fade_in) gauge_fade_in->start();
            anims_loaded = true;
        }

        if (state == ResultState::RAINBOW) {
            if (rainbow_start_ms < 0) rainbow_start_ms = current_ms;
            rainbow_frac = (float)fmod((current_ms - rainbow_start_ms) / kRainbowCelMs, 8.0);
        }
        if (tamashii_fire_change) tamashii_fire_change->update(current_ms);
        if (gauge_fade_in) gauge_fade_in->update(current_ms);
        return;
    }

    if (mode == GaugeMode::DAN && !anims_loaded) {
        tamashii_fire_change = (TextureChangeAnimation*)tex.get_animation(25);
        gauge_update_anim    = (FadeAnimation*)tex.get_animation(10);
        anims_loaded = true;
    }

    if (mode == GaugeMode::NORMAL && chn_model) {
        is_rainbow = (soul >= 10000.0);
        is_clear   = (soul >= (double)norma);
    } else {
        is_rainbow = (gauge_length == gauge_max);
        is_clear   = (mode == GaugeMode::NORMAL)
                     ? gauge_length > clear_start[std::min(difficulty, (int)Difficulty::HARD)] - 1
                     : is_rainbow;
    }

    if (gauge_length == gauge_max && !rainbow_fade_in.has_value()) {
        rainbow_fade_in = (FadeAnimation*)tex.get_animation(63);
        rainbow_fade_in.value()->start();
        rainbow_start_ms = current_ms;
    }

    if (gauge_update_anim)    gauge_update_anim->update(current_ms);
    if (tamashii_fire_change) tamashii_fire_change->update(current_ms);

    if (rainbow_fade_in.has_value()) {
        rainbow_fade_in.value()->update(current_ms);
        rainbow_frac = (float)fmod((current_ms - rainbow_start_ms) / kRainbowCelMs, 8.0);
    }
}

void Gauge::draw(float y) {
    if (mode == GaugeMode::NORMAL) {
        bool mirrored = y > tex.screen_height / 2.0f;
        Mirror mirror = mirrored ? Mirror::VERTICAL : Mirror::NONE;

        tex.draw_texture(tex.get_enum("gauge/border" + string_diff), {.mirror = mirror, .y = y, .index = mirrored});

        tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_unfilled" + string_diff)),
                         {.mirror = mirror, .y = y, .index = mirrored});

        constexpr int   kSegs   = 50;
        constexpr float kPitch  = 21.0f;
        constexpr float kX0     = 738.0f;
        static const int kClearCell[3] = {30, 35, 40};

        const int art  = chn_model ? art_index : std::min(2, difficulty);
        const int cs   = kClearCell[std::max(0, std::min(2, art))];
        int cell = (int)((gauge_length / gauge_max) * (float)kSegs);
        if (cell < 0)     cell = 0;
        if (cell > kSegs) cell = kSegs;

        float bar_width      = tex.textures[tex.get_enum("gauge/" + std::to_string((int)player_num) + "p_bar")]->width;
        const int red_cells  = std::min(cell, cs - 1);

        if (red_cells > 0)
            tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_bar")),
                             {.y = y, .x2 = kPitch * red_cells - bar_width, .index = mirrored});

        if (cell >= cs) {
            tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION,
                             {.mirror = mirror, .x = kX0 + kPitch * (cs - 1) - 741.0f,
                              .y = y, .index = mirrored});
            const int gold_cells = cell - cs;
            if (gold_cells > 0) {
                const float gw = kPitch * gold_cells - bar_width;
                tex.draw_texture(GAUGE::BAR_CLEAR_TOP,
                                 {.mirror = mirror, .x = kPitch * cs, .y = y,
                                  .x2 = gw, .index = mirrored});
                tex.draw_texture(GAUGE::BAR_CLEAR_BOTTOM,
                                 {.x = kPitch * cs, .y = y,
                                  .x2 = gw, .index = mirrored});
            }
        }

        if (cell == kSegs && rainbow_fade_in.has_value()) {
            float fade    = rainbow_fade_in.value()->attribute;
            int   frame_a = (int)rainbow_frac % 8;
            int   frame_b = (frame_a + 1) % 8;
            float t       = rainbow_frac - (int)rainbow_frac;
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                             {.frame = frame_a, .mirror = mirror, .y = y, .fade = fade, .index = mirrored});
            tex.draw_texture(tex.get_enum("gauge/rainbow" + string_diff),
                             {.frame = frame_b, .mirror = mirror, .y = y, .fade = fade * t, .index = mirrored});
        }

        const int prev_cell = (int)((previous_length / gauge_max) * (float)kSegs);
        if (cell <= kSegs && cell > prev_cell) {
            const float fx = kX0 + kPitch * cell - 24.0f - 732.0f;
            if (cell == cs) {
                tex.draw_texture(GAUGE::BAR_CLEAR_TRANSITION_FADE,
                                 {.mirror = mirror, .x = fx, .y = y,
                                  .fade = gauge_update_anim->attribute, .index = mirrored});
            } else if (cell > cs) {
                tex.draw_texture(GAUGE::BAR_CLEAR_FADE,
                                 {.x = fx, .y = y,
                                  .fade = gauge_update_anim->attribute, .index = mirrored});
            } else {
                tex.draw_texture(tex.get_enum("gauge/" + (std::to_string((int)player_num) + "p_bar_fade")),
                                 {.x = fx, .y = y,
                                  .fade = gauge_update_anim->attribute, .index = mirrored});
            }
        }

        tex.draw_texture(tex.get_enum("gauge/overlay" + string_diff),
                         {.mirror = mirror, .y = y, .fade = kOverlayAlpha, .index = mirrored});

        int label_index = chn_model ? art_index : std::min(2, difficulty);
        bool live_label_lit = chn_model ? is_clear : (cell >= cs);
        if (live_label_lit) {
            tex.draw_texture(tex.get_enum("gauge/clear_" + global_data.config->general.language),
                             {.y = y, .index = label_index + (mirrored * 3)});
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
                             {.y = y, .index = label_index + (mirrored * 3)});
            tex.draw_texture(GAUGE::TAMASHII_DARK, {.y = y, .index = mirrored});
        }
    } else {
        // DAN mode
        if (!anims_loaded) return;

        constexpr int   kDanSegs  = 50;
        constexpr float kDanPitch = 21.0f;
        constexpr float kDanX0    = 738.0f;
        constexpr float kDanBaseX = 726.0f;
        constexpr float kDanShift = kDanX0 - kDanBaseX;

        int dan_n = (int)((gauge_length / gauge_max) * kDanSegs);
        if (dan_n > kDanSegs) dan_n = kDanSegs;
        float bar_width  = (float)tex.textures[GAUGE_DAN::_1P_BAR]->width;
        float length_px  = dan_n * kDanPitch;

        tex.draw_texture(GAUGE_DAN::BORDER,      {});
        tex.draw_texture(GAUGE_DAN::_1P_UNFILLED, {});
        tex.draw_texture(GAUGE_DAN::_1P_BAR,     {.x = kDanShift, .x2 = length_px - bar_width});

        if (is_rainbow && rainbow_fade_in.has_value()) {
            float fade    = rainbow_fade_in.value()->attribute;
            int   frame_a = (int)rainbow_frac % 8;
            int   frame_b = (frame_a + 1) % 8;
            float t       = rainbow_frac - (int)rainbow_frac;
            tex.draw_texture(GAUGE_DAN::RAINBOW, {.frame = frame_a, .fade = fade});
            tex.draw_texture(GAUGE_DAN::RAINBOW, {.frame = frame_b, .fade = fade * t});
        }

        if ((int)gauge_length <= (int)gauge_max && (int)gauge_length > (int)previous_length) {
            const float dan_fade_w = (float)tex.textures[GAUGE_DAN::_1P_BAR_FADE]->width;
            tex.draw_texture(GAUGE_DAN::_1P_BAR_FADE,
                             {.x = (kDanX0 + length_px - dan_fade_w) - 716.0f,
                              .fade = gauge_update_anim->attribute});
        }

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

void Gauge::draw_result(double external_fade) {
    if (!anims_loaded) return;

    // NORMAL mode uses its own internal fade; DAN mode uses the caller-supplied fade
    double base_fade    = (mode == GaugeMode::NORMAL) ? gauge_fade_in->attribute : external_fade;
    double rainbow_fade = std::min((double)gauge_fade_in->attribute, external_fade);

    std::string player_str = std::to_string(static_cast<int>(player_num)) + "p";
    float scale = result_scale;

    if (mode == GaugeMode::NORMAL) {
        int gauge_length_int = (int)gauge_length;
        int clear_point      = chn_model ? clear_start[art_index] : clear_start[difficulty];
        int label_index      = chn_model ? art_index : (int)difficulty;
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

        bool label_lit = chn_model ? (state == ResultState::CLEAR || state == ResultState::RAINBOW)
                                   : (gauge_length_int >= clear_point - 1);
        if (label_lit) {
            tex.draw_texture(tex.get_enum("gauge/clear_" + global_data.config->general.language),
                             {.scale = scale, .fade = base_fade, .index = label_index + (is_2p * 3)});
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
                             {.scale = scale, .fade = base_fade, .index = label_index + (is_2p * 3)});
            tex.draw_texture(GAUGE::TAMASHII_DARK, {.scale = scale, .fade = base_fade, .index = is_2p});
        }
    } else { // DAN mode
        float bar_width = (float)tex.textures[GAUGE::_1P_BAR]->width;
        float length_px = gauge_length * bar_width;

        tex.draw_texture(GAUGE::_1P_UNFILLED, {.fade = base_fade});

        if (state != ResultState::RAINBOW)
            tex.draw_texture(GAUGE::_1P_BAR, {.x2 = length_px - bar_width, .fade = base_fade});

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
