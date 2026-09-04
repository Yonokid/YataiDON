#include "sandbox.h"
#include "../objects/sandbox/fixtures.h"
#include "../objects/sandbox/fixtures_title.h"
#include "../objects/sandbox/fixtures_result.h"
#include "../objects/sandbox/fixtures_song_select.h"
#include "../objects/sandbox/fixtures_global.h"
#include "../objects/sandbox/fixtures_entry.h"
#include "../libs/input.h"

static constexpr int SB_PANEL_W       = 220;
static constexpr int SB_ITEM_H        = 32;
static constexpr int SB_HEADER_H      = 20;
static constexpr int SB_BTN_H         = 28;
static constexpr int SB_BTN_MARG      = 8;
static constexpr int SB_TYPE_H        = 24;
static constexpr int SCROLL_TOP       = 46;
static constexpr int MIN_SCROLL_AREA_H = 60;
static constexpr int TYPE_AREA_MAX_H  = 120;

// ─── Panel layout helpers ─────────────────────────────────────────────────────

struct PanelEntry {
    bool        is_header;
    int         fixture_idx;  // -1 for headers
    std::string label;
    int         content_y;   // y in content-space (before scroll)
};

static std::vector<PanelEntry> build_panel_layout(
    const std::vector<std::unique_ptr<SandboxScreen::Fixture>>& fixtures)
{
    std::vector<PanelEntry> layout;
    std::string prev_screen;
    int y = 0;
    for (int i = 0; i < (int)fixtures.size(); ++i) {
        const auto& f = fixtures[i];
        if (f->screen != prev_screen) {
            layout.push_back({true, -1, f->screen, y});
            y += SB_HEADER_H;
            prev_screen = f->screen;
        }
        layout.push_back({false, i, f->name, y});
        y += SB_ITEM_H;
    }
    return layout;
}

static int panel_content_height(const std::vector<PanelEntry>& layout) {
    if (layout.empty()) return 0;
    const auto& last = layout.back();
    return last.content_y + (last.is_header ? SB_HEADER_H : SB_ITEM_H);
}

// ─── Shared bottom-layout geometry ───────────────────────────────────────────

struct PanelGeometry {
    int btn_y_pause;
    int var_btn_y;
    int type_row_h;
    int n_types;
    int type_content_h;   // total pixel height of all type rows
    int type_area_h;      // visible height of the type scroll area (clamped)
    int type_area_top;    // screen-y where the type area begins
    int y_types_start;    // screen-y of first type row (before type_scroll applied)
    int scroll_bottom;    // screen-y of the bottom of the fixture scroll area
    int scroll_area_h;    // height of the fixture scroll area
};

static PanelGeometry compute_geometry(int screen_height, int n_types) {
    PanelGeometry g;
    g.type_row_h     = SB_TYPE_H + 2;
    g.n_types        = n_types;
    g.btn_y_pause    = screen_height - 16 - 4 - SB_BTN_H;
    g.var_btn_y      = g.btn_y_pause - 4 - SB_BTN_H;

    // Total content height of the type list (with top/bottom padding gaps)
    g.type_content_h = n_types > 0 ? n_types * g.type_row_h + 8 + 8 : 0;
    g.type_area_h    = std::min(g.type_content_h, TYPE_AREA_MAX_H);

    // Push scroll_bottom up by the visible type area; clamp so fixture list
    // always has at least MIN_SCROLL_AREA_H pixels.
    const int ideal_scroll_bottom = g.var_btn_y - 4 - g.type_area_h;
    g.scroll_bottom  = std::max(SCROLL_TOP + MIN_SCROLL_AREA_H, ideal_scroll_bottom);
    g.scroll_area_h  = g.scroll_bottom - SCROLL_TOP;

    g.type_area_top  = g.scroll_bottom + 8;
    g.y_types_start  = g.type_area_top;  // rows offset by -type_scroll at draw/hit-test time

    return g;
}

// ─────────────────────────────────────────────────────────────────────────────

void SandboxScreen::ensure_screen_loaded(const std::string& screen_name) {
    if (screen_name == "global" || screen_name == loaded_screen) return;
    try {
        tex.load_screen_textures(screen_name);
        audio.load_screen_sounds(screen_name);
    } catch (const std::exception& e) {
        spdlog::warn("Sandbox: could not load {} assets: {}", screen_name, e.what());
    }
    loaded_screen = screen_name;
}

void SandboxScreen::on_screen_start() {
    ray::ShowCursor();
    loaded_screen = "game";
    try {
        tex.load_screen_textures("game");
        audio.load_screen_sounds("game");
    } catch (const std::exception& e) {
        spdlog::warn("Sandbox: could not load game assets: {}", e.what());
    }
    spdlog::info("Sandbox screen initialized");

    fixtures.clear();

    // ── game ──────────────────────────────────────────────────────────────────
    fixtures.push_back(std::make_unique<JudgmentFixture>());
    fixtures.push_back(std::make_unique<DrumHitFixture>());
    fixtures.push_back(std::make_unique<ComboFixture>());
    fixtures.push_back(std::make_unique<LaneHitEffectFixture>());
    fixtures.push_back(std::make_unique<GaugeHitEffectFixture>());
    fixtures.push_back(std::make_unique<GogoTimeFixture>());
    fixtures.push_back(std::make_unique<ScoreCounterFixture>());
    fixtures.push_back(std::make_unique<DrumrollCounterFixture>());
    fixtures.push_back(std::make_unique<BalloonCounterFixture>());
    fixtures.push_back(std::make_unique<ComboAnnounceFixture>());
    fixtures.push_back(std::make_unique<FCAnimationFixture>());
    fixtures.push_back(std::make_unique<ClearAnimationFixture>());
    fixtures.push_back(std::make_unique<FailAnimationFixture>());
    fixtures.push_back(std::make_unique<FireworksFixture>());
    fixtures.push_back(std::make_unique<BranchIndicatorFixture>());
    fixtures.push_back(std::make_unique<GaugeFixture>());
    fixtures.push_back(std::make_unique<JudgeCounterFixture>());
    fixtures.push_back(std::make_unique<KusudamaCounterFixture>());
    fixtures.push_back(std::make_unique<NoteArcFixture>());
    fixtures.push_back(std::make_unique<ResultTransitionFixture>());
    fixtures.push_back(std::make_unique<TransitionFixture>());
    fixtures.push_back(std::make_unique<ScoreCounterAnimFixture>());
    fixtures.push_back(std::make_unique<SongInfoFixture>());

    // ── title ─────────────────────────────────────────────────────────────────
    fixtures.push_back(std::make_unique<WarningXFixture>());
    fixtures.push_back(std::make_unique<WarningBachiHitFixture>());
    fixtures.push_back(std::make_unique<AttractCameraFixture>());
    fixtures.push_back(std::make_unique<BanaAdvertisementFixture>());
    fixtures.push_back(std::make_unique<CameraCloudFixture>());

    // ── result ────────────────────────────────────────────────────────────────
    fixtures.push_back(std::make_unique<ResultBackgroundFixture>());
    fixtures.push_back(std::make_unique<ResultCrownFixture>());
    fixtures.push_back(std::make_unique<ResultFadeInFixture>());
    fixtures.push_back(std::make_unique<BottomCharactersFixture>());
    fixtures.push_back(std::make_unique<HighScoreIndicatorFixture>());

    // ── entry ─────────────────────────────────────────────────────────────────
    fixtures.push_back(std::make_unique<CostumeMenuFixture>());

    // ── song_select ───────────────────────────────────────────────────────────
    fixtures.push_back(std::make_unique<DanTransitionFixture>());
    fixtures.push_back(std::make_unique<UraSwitchFixture>());

    // ── global ────────────────────────────────────────────────────────────────
    fixtures.push_back(std::make_unique<Chara3DFixture>());
    fixtures.push_back(std::make_unique<AllNetIconFixture>());
    fixtures.push_back(std::make_unique<CoinOverlayFixture>());
    fixtures.push_back(std::make_unique<EntryOverlayFixture>());
    fixtures.push_back(std::make_unique<IndicatorFixture>());
    fixtures.push_back(std::make_unique<NameplateFixture>());
    fixtures.push_back(std::make_unique<TimerFixture>());

    current_ms = fixture_start_ms = get_current_ms();

    for (auto& f : fixtures) {
        if (f->screen == loaded_screen) f->reset(current_ms);
    }

    fixture_idx  = 0;
    panel_scroll = 0;
    type_scroll  = 0;
}

Screens SandboxScreen::on_screen_end(Screens next_screen) {
    fixtures.clear();
    ray::HideCursor();
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> SandboxScreen::handle_input() {
    if (ray::IsKeyPressed(ray::KEY_ESCAPE))
        return on_screen_end(Screens::TITLE);

    if (fixtures.empty()) return std::nullopt;

    ray::Vector2 mouse = ray::GetMousePosition();
    bool lclick        = ray::IsMouseButtonPressed(ray::MOUSE_BUTTON_LEFT);
    bool rclick        = ray::IsMouseButtonPressed(ray::MOUSE_BUTTON_RIGHT);

    if (ray::IsKeyPressed(ray::KEY_SPACE)) {
        current_ms = get_current_ms();
        fixtures[fixture_idx]->on_space(current_ms);
    }
    if (ray::IsKeyPressed(ray::KEY_R)) {
        current_ms = fixture_start_ms = get_current_ms();
        fixtures[fixture_idx]->reset(current_ms);
    }

    auto layout              = build_panel_layout(fixtures);
    auto panel_type_list     = fixtures[fixture_idx]->type_names();
    auto g                   = compute_geometry(tex.screen_height, (int)panel_type_list.size());
    const int content_h      = panel_content_height(layout);
    const int max_scroll     = std::max(0, content_h - g.scroll_area_h);
    const int max_type_scroll = std::max(0, g.type_content_h - g.type_area_h);

    // Mouse-wheel scroll over panel
    if (mouse.x < SB_PANEL_W) {
        float wheel = ray::GetMouseWheelMove();
        if (wheel != 0.0f) {
            // Route wheel to type area or fixture list based on cursor position
            if (g.n_types > 0 && mouse.y >= g.type_area_top && mouse.y < g.var_btn_y)
                type_scroll = std::max(0, std::min(type_scroll - (int)(wheel * SB_TYPE_H), max_type_scroll));
            else
                panel_scroll = std::max(0, std::min(panel_scroll - (int)(wheel * SB_ITEM_H), max_scroll));
        }
    }

    if (lclick && mouse.x < SB_PANEL_W) {
        // Pause button (fixed)
        if (mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
            mouse.y >= g.btn_y_pause && mouse.y < g.btn_y_pause + SB_BTN_H)
            paused = !paused;

        // Variant button (fixed)
        if (mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
            mouse.y >= g.var_btn_y && mouse.y < g.var_btn_y + SB_BTN_H)
            fixtures[fixture_idx]->on_tab(current_ms);

        // Type buttons — hit-test in scissored screen-space
        if (g.n_types > 0 && mouse.y >= g.type_area_top && mouse.y < g.type_area_top + g.type_area_h) {
            int cy = (int)mouse.y - g.type_area_top + type_scroll;
            int i  = cy / g.type_row_h;
            if (i >= 0 && i < g.n_types)
                fixtures[fixture_idx]->set_type(i, current_ms);
        }

        // Fixture list (scrollable) — only non-header entries are clickable
        if (mouse.y >= SCROLL_TOP && mouse.y < g.scroll_bottom) {
            int cy = (int)mouse.y - SCROLL_TOP + panel_scroll;
            for (const auto& entry : layout) {
                if (entry.is_header) continue;
                if (cy >= entry.content_y && cy < entry.content_y + SB_ITEM_H) {
                    int new_idx = entry.fixture_idx;
                    if (new_idx != fixture_idx) {
                        ensure_screen_loaded(fixtures[new_idx]->screen);
                        fixture_idx  = new_idx;
                        type_scroll  = 0;  // reset type scroll for new fixture
                        current_ms   = fixture_start_ms = get_current_ms();
                        fixtures[fixture_idx]->reset(current_ms);
                    }
                    break;
                }
            }
        }
    }

    if (mouse.x >= SB_PANEL_W) {
        if (lclick) {
            current_ms = get_current_ms();
            fixtures[fixture_idx]->on_space(current_ms);
        }
        if (rclick) {
            current_ms = fixture_start_ms = get_current_ms();
            fixtures[fixture_idx]->reset(current_ms);
        }
    }

    if (is_l_kat_pressed()) {
        current_ms -= FRAME_MS;
        if (current_ms < fixture_start_ms) current_ms = fixture_start_ms;
        fixtures[fixture_idx]->update(current_ms);
    } else if (is_r_kat_pressed()) {
        current_ms += FRAME_MS;
        fixtures[fixture_idx]->update(current_ms);
    }

    return std::nullopt;
}

std::optional<Screens> SandboxScreen::update() {
    Screen::update();

    auto nav = handle_input();
    if (nav.has_value()) return nav;

    if (!fixtures.empty() && !paused) {
        current_ms = get_current_ms();
        fixtures[fixture_idx]->update(current_ms);
    }
    return std::nullopt;
}

void SandboxScreen::draw_panel() const {
    ray::DrawRectangle(0, 0, SB_PANEL_W, tex.screen_height, ray::Color{20, 20, 30, 230});
    ray::DrawLine(SB_PANEL_W, 0, SB_PANEL_W, tex.screen_height, ray::Color{80, 80, 100, 255});

    ray::Vector2 mouse = ray::GetMousePosition();

    // Fixed header
    ray::DrawText("SANDBOX", 10, 10, 20, ray::WHITE);
    ray::DrawLine(0, 36, SB_PANEL_W, 36, ray::Color{80, 80, 100, 255});

    auto type_list           = fixtures[fixture_idx]->type_names();
    auto g                   = compute_geometry(tex.screen_height, (int)type_list.size());
    const int max_type_scroll = std::max(0, g.type_content_h - g.type_area_h);

    // ── Scrollable fixture list ───────────────────────────────────────────────
    auto layout          = build_panel_layout(fixtures);
    const int content_h  = panel_content_height(layout);
    const int max_scroll = std::max(0, content_h - g.scroll_area_h);

    ray::BeginScissorMode(0, SCROLL_TOP, SB_PANEL_W, g.scroll_area_h);
    for (const auto& entry : layout) {
        int iy = SCROLL_TOP + entry.content_y - panel_scroll;

        if (entry.is_header) {
            ray::DrawRectangle(0, iy, SB_PANEL_W, SB_HEADER_H, ray::Color{35, 35, 55, 255});
            std::string label = entry.label;
            for (auto& c : label) c = (char)toupper((unsigned char)c);
            ray::DrawText(label.c_str(), 8, iy + (SB_HEADER_H - 11) / 2, 11,
                          ray::Color{140, 160, 220, 255});
        } else {
            bool selected = (entry.fixture_idx == fixture_idx);
            bool hovered  = !selected && mouse.x < SB_PANEL_W &&
                            mouse.y >= iy && mouse.y < iy + SB_ITEM_H;
            if (selected)
                ray::DrawRectangle(0, iy, SB_PANEL_W, SB_ITEM_H, ray::Color{60, 80, 180, 220});
            else if (hovered)
                ray::DrawRectangle(0, iy, SB_PANEL_W, SB_ITEM_H, ray::Color{40, 50, 100, 160});
            ray::Color col = selected ? ray::WHITE
                           : (hovered ? ray::Color{220, 220, 255, 255}
                                      : ray::Color{180, 180, 180, 255});
            ray::DrawText(entry.label.c_str(), 10, iy + 7, 16, col);
        }
    }
    ray::EndScissorMode();

    // Fixture list scrollbar
    if (max_scroll > 0) {
        float ratio   = (float)g.scroll_area_h / content_h;
        int   thumb_h = std::max(16, (int)(g.scroll_area_h * ratio));
        int   thumb_y = SCROLL_TOP + (int)((g.scroll_area_h - thumb_h) * (float)panel_scroll / max_scroll);
        ray::DrawRectangle(SB_PANEL_W - 3, thumb_y, 2, thumb_h, ray::Color{120, 130, 160, 180});
    }

    // ── Fixed bottom: separator → types → variant → pause ────────────────────
    ray::DrawLine(0, g.scroll_bottom + 4, SB_PANEL_W, g.scroll_bottom + 4, ray::Color{80, 80, 100, 255});

    // Type buttons (scrollable)
    if (g.n_types > 0) {
        ray::BeginScissorMode(0, g.type_area_top, SB_PANEL_W, g.type_area_h);
        for (int i = 0; i < g.n_types; ++i) {
            int ty   = g.y_types_start + i * g.type_row_h - type_scroll;
            bool sel = (i == fixtures[fixture_idx]->get_type());
            // Only hover if the row is actually within the visible scissored area
            bool hov = !sel && mouse.x < SB_PANEL_W &&
                       mouse.y >= g.type_area_top && mouse.y < g.type_area_top + g.type_area_h &&
                       mouse.y >= ty && mouse.y < ty + SB_TYPE_H;
            if (sel)
                ray::DrawRectangle(SB_BTN_MARG, ty, SB_PANEL_W - SB_BTN_MARG * 2, SB_TYPE_H,
                                   ray::Color{60, 80, 180, 220});
            else if (hov)
                ray::DrawRectangle(SB_BTN_MARG, ty, SB_PANEL_W - SB_BTN_MARG * 2, SB_TYPE_H,
                                   ray::Color{40, 50, 100, 160});
            ray::Color tcol = sel ? ray::WHITE
                            : (hov ? ray::Color{220, 220, 255, 255}
                                   : ray::Color{180, 180, 180, 255});
            ray::DrawText(type_list[i].c_str(), SB_BTN_MARG + 4, ty + (SB_TYPE_H - 12) / 2, 12, tcol);
        }
        ray::EndScissorMode();

        // Type area scrollbar
        if (max_type_scroll > 0) {
            float ratio   = (float)g.type_area_h / g.type_content_h;
            int   thumb_h = std::max(10, (int)(g.type_area_h * ratio));
            int   thumb_y = g.type_area_top + (int)((g.type_area_h - thumb_h) * (float)type_scroll / max_type_scroll);
            ray::DrawRectangle(SB_PANEL_W - 3, thumb_y, 2, thumb_h, ray::Color{120, 130, 160, 180});
        }

        int y_types_end = g.type_area_top + g.type_area_h;
        ray::DrawLine(0, y_types_end + 4, SB_PANEL_W, y_types_end + 4, ray::Color{80, 80, 100, 255});
    }

    // Variant button
    bool var_hov = mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
                   mouse.y >= g.var_btn_y && mouse.y < g.var_btn_y + SB_BTN_H;
    ray::DrawRectangle(SB_BTN_MARG, g.var_btn_y, SB_PANEL_W - SB_BTN_MARG * 2, SB_BTN_H,
                       ray::Color{80, 80, 160, static_cast<unsigned char>(var_hov ? 255 : 200)});
    ray::DrawText("VARIANT", SB_BTN_MARG + 6, g.var_btn_y + 7, 14, ray::WHITE);

    // Pause button
    bool btn_hov = mouse.x >= SB_BTN_MARG && mouse.x < SB_PANEL_W - SB_BTN_MARG &&
                   mouse.y >= g.btn_y_pause && mouse.y < g.btn_y_pause + SB_BTN_H;
    ray::Color btn_col = paused
        ? ray::Color{160, 50,  50,  static_cast<unsigned char>(btn_hov ? 255 : 210)}
        : ray::Color{50,  130, 50,  static_cast<unsigned char>(btn_hov ? 255 : 210)};
    ray::DrawRectangle(SB_BTN_MARG, g.btn_y_pause, SB_PANEL_W - SB_BTN_MARG * 2, SB_BTN_H, btn_col);
    ray::DrawText(paused ? "|| PAUSED" : ">  PLAYING", SB_BTN_MARG + 6, g.btn_y_pause + 7, 14, ray::WHITE);

    ray::DrawText("ESC: title", 6, tex.screen_height - 14, 11, ray::Color{120, 120, 130, 255});
}

void SandboxScreen::draw_debug() const {
    if (fixtures.empty()) return;
    const auto& f = fixtures[fixture_idx];

    int x = SB_PANEL_W + 8;
    int y = 8;

    if (paused) {
        ray::DrawText("[PAUSED]", x, y, 14, ray::Color{255, 220, 60, 255});
        x += ray::MeasureText("[PAUSED]", 14) + 8;
    }
    std::string t_str = "t = " + std::to_string((int)(current_ms - fixture_start_ms)) + " ms";
    ray::DrawText(t_str.c_str(), x, y, 14, ray::Color{180, 180, 180, 255});
    x = SB_PANEL_W + 8;
    y += 20;

    for (const auto& line : f->debug_lines()) {
        ray::DrawText(line.c_str(), x, y, 14, ray::Color{180, 210, 255, 255});
        y += 18;
    }

    if (paused)
        ray::DrawText(",/.: step frame", SB_PANEL_W + 8, tex.screen_height - 32, 11,
                      ray::Color{255, 220, 60, 200});
    ray::DrawText(f->controls().c_str(), SB_PANEL_W + 8, tex.screen_height - 18, 12,
                  ray::Color{120, 130, 150, 255});
}

void SandboxScreen::draw() {
    constexpr int TILE = 32;
    for (int ty = 0; ty < tex.screen_height; ty += TILE) {
        for (int tx = 0; tx < tex.screen_width; tx += TILE) {
            ray::Color bg = ((tx / TILE + ty / TILE) % 2 == 0)
                ? ray::Color{10, 10, 20, 255}
                : ray::Color{18, 18, 30, 255};
            ray::DrawRectangle(tx, ty, TILE, TILE, bg);
        }
    }

    if (!fixtures.empty()) {
        auto& f = fixtures[fixture_idx];
        uint32_t anchor = f->anchor_texture_id();

        const float vcx = SB_PANEL_W + (tex.screen_width  - SB_PANEL_W) / 2.0f;
        const float vcy =               tex.screen_height / 2.0f;
        auto it = tex.textures.find(anchor);
        if (it != tex.textures.end()) {
            tex.draw_offset_x = vcx - (float)it->second->x[0] - (float)it->second->width / 2.0f;
            tex.draw_offset_y = vcy - (float)it->second->y[0] - (float)it->second->height / 2.0f;
        }

        f->draw();

        tex.draw_offset_x = 0.0f;
        tex.draw_offset_y = 0.0f;
    }

    draw_panel();
    draw_debug();
}
