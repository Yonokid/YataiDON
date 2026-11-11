#include "ime_ui.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <string>
#include <vector>

#include "../gfx/text.h"
#include "../gfx/texture.h"
#include "../global_data.h"

#if defined(YATAIDON_HAS_SDL_TEXT_INPUT)
#include "../gfx/sdl_compat.h"
#if !defined(YATAIDON_USE_SDL2)
#define YATAIDON_IME_UI 1
#endif
#endif

namespace {
    struct ImeState {
        std::string composition;
        int start = 0;  // cursor / selected clause, in code points
        int length = 0; // selected clause length, 0 = plain cursor
        std::vector<std::string> candidates;
        int selected = -1;
        bool horizontal = false;
        int page_size = 0; // largest page seen since the list opened
    };

    std::mutex ime_mutex; // the watch runs where SDL events are pumped / pushed
    ImeState ime;
    bool ime_active = false;
    bool field_set = false;
    Rect field{};
    std::array<int, 4> last_area{-1, -1, -1, -1};

    // Byte offset of code point `cp` in `s` (clamped to its end).
    size_t utf8_offset(const std::string& s, int cp) {
        size_t i = 0;
        for (int n = 0; n < cp && i < s.size(); ++n) {
            ++i;
            while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
                ++i;
            }
        }
        return i;
    }

#ifdef YATAIDON_IME_UI
    bool SDLCALL ime_event_watch(void* /*userdata*/, SDL_Event* event) {
        switch (event->type) {
        case SDL_EVENT_TEXT_EDITING: {
            std::scoped_lock lock(ime_mutex);
            ime.composition = event->edit.text ? event->edit.text : "";
            ime.start = std::max(0, static_cast<int>(event->edit.start));
            ime.length = std::max(0, static_cast<int>(event->edit.length));
            break;
        }
        case SDL_EVENT_TEXT_EDITING_CANDIDATES: {
            const auto& c = event->edit_candidates;
            std::scoped_lock lock(ime_mutex);
            ime.candidates.clear();
            for (int i = 0; c.candidates && i < c.num_candidates; ++i) {
                ime.candidates.emplace_back(c.candidates[i] ? c.candidates[i] : "");
            }
            ime.selected = c.selected_candidate;
            ime.horizontal = c.horizontal;
            ime.page_size =
                ime.candidates.empty() ? 0 : std::max(ime.page_size, static_cast<int>(ime.candidates.size()));
            break;
        }
        case SDL_EVENT_TEXT_INPUT: {
            std::scoped_lock lock(ime_mutex);
            ime.composition.clear(); // committed
            ime.start = ime.length = 0;
            break;
        }
        default:
            break;
        }
        return true;
    }

    // Where any OS IME UI that still shows should go: the field, in window coordinates.
    void update_text_input_area(const Camera2D& camera, Rect r) {
        SDL_Window* win = main_window();
        if (!win) {
            return;
        }
        Vec2 a = get_world_to_screen_2d({r.x, r.y}, camera);
        Vec2 b = get_world_to_screen_2d({r.x + r.width, r.y + r.height}, camera);
        int ww = 0;
        int wh = 0;
        int pw = 0;
        int ph = 0;
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
        const float kx = pw > 0 ? static_cast<float>(ww) / pw : 1.0f;
        const float ky = ph > 0 ? static_cast<float>(wh) / ph : 1.0f;
        const std::array<int, 4> area{static_cast<int>(a.x * kx), static_cast<int>(a.y * ky),
                                      static_cast<int>((b.x - a.x) * kx), static_cast<int>((b.y - a.y) * ky)};
        if (area != last_area) {
            const SDL_Rect rect{area[0], area[1], area[2], area[3]};
            SDL_SetTextInputArea(win, &rect, 0);
            last_area = area;
        }
    }
#else
    void update_text_input_area(const Camera2D&, Rect) {}
#endif
} // namespace

void ime_ui_set_hint() {
#ifdef YATAIDON_IME_UI
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition,candidates");
#endif
}

void ime_ui_install() {
#ifdef YATAIDON_IME_UI
    SDL_AddEventWatch(ime_event_watch, nullptr);
#endif
}

void ime_ui_text_input_changed(bool active) {
    std::scoped_lock lock(ime_mutex);
    ime_active = active;
    ime = ImeState{};
    last_area = {-1, -1, -1, -1};
}

void ime_ui_set_field(Rect rect) {
    field = rect;
    field_set = true;
}

void ime_ui_draw(const Camera2D& camera) {
    const bool had_field = field_set;
    field_set = false;
    if (!ime_active) {
        return;
    }
    const float S = tex.screen_scale;
    const auto sw = static_cast<float>(tex.screen_width);
    const auto sh = static_cast<float>(tex.screen_height);
    const Rect r = had_field ? field : Rect{sw / 4, sh * 0.3f, sw / 2, 60 * S};

    update_text_input_area(camera, r);

    ImeState st;
    {
        std::scoped_lock lock(ime_mutex);
        st = ime;
    }
    if (st.composition.empty() && st.candidates.empty()) {
        return;
    }

    // Windows SDL numbers the candidates itself ("1 xx"); elsewhere they come bare.
    std::vector<std::string> labels;
    labels.reserve(st.candidates.size());
    for (size_t i = 0; i < st.candidates.size(); ++i) {
        const std::string& c = st.candidates[i];
        const bool numbered = c.size() >= 2 && c[0] >= '0' && c[0] <= '9' && c[1] == ' ';
        labels.push_back(numbered ? c : std::to_string((i + 1) % 10).append(" ").append(c));
    }
    // SDL's Windows backend reports the selection over the whole list, the strings for one page.
    int sel = st.selected;
    int page = 0;
    if (sel >= static_cast<int>(labels.size()) && st.page_size > 0) {
        page = sel / st.page_size;
        sel -= page * st.page_size;
    }
    const std::string page_label = page > 0 ? "p." + std::to_string(page + 1) : "";

    const int fs = std::max(8, static_cast<int>(30 * S));
    const float spacing = 1.0f;
    std::string all = st.composition + page_label;
    for (const auto& l : labels) {
        all += l;
    }
    Font font = font_manager.get_font(all, fs);
    auto measure = [&](const std::string& s) {
        return measure_text_ex(font, s.c_str(), static_cast<float>(fs), spacing).x;
    };

    const float pad = 12 * S;
    const float row_h = fs * 1.25f;
    const float gap = 22 * S;
    const float margin = 8 * S;
    const float comp_w = st.composition.empty() ? 0 : measure(st.composition);
    const float comp_h = st.composition.empty() ? 0 : row_h;
    std::vector<float> label_w;
    label_w.reserve(labels.size());
    for (const auto& l : labels) {
        label_w.push_back(measure(l));
    }
    const bool horizontal = st.horizontal;
    float cand_w = 0;
    float cand_h = 0;
    if (!labels.empty()) {
        for (float lw : label_w) {
            cand_w = horizontal ? cand_w + lw + gap : std::max(cand_w, lw);
        }
        cand_w -= horizontal ? gap : 0;
        cand_h = horizontal ? row_h : row_h * labels.size();
        if (!page_label.empty()) {
            cand_h += row_h * 0.8f;
        }
    }
    const float sep = (comp_h > 0 && cand_h > 0) ? 10 * S : 0;
    const float w = std::max({comp_w, cand_w + 2 * pad, 160 * S}) + 2 * pad;
    const float h = comp_h + sep + cand_h + 2 * pad;
    const float below = sh - margin - (r.y + r.height + 10 * S);
    const float above = r.y - 10 * S - margin;
    const float x = std::clamp(r.x + r.width / 2 - w / 2, margin, std::max(margin, sw - w - margin));
    float y = r.y + r.height + 10 * S;
    if (h > below) { // not below the field: above it, else as far down as fits
        y = h <= above ? r.y - h - 10 * S : std::max(margin, sh - margin - h);
    }

    const Color border{119, 26, 45, 255}; // the default search box's outline
    const Rect panel{x, y, w, h};
    const float round = std::min(24 * S / std::min(w, h), 1.0f);
    draw_rectangle_rounded(panel, round, 16, Color{255, 255, 255, 245});
    draw_rectangle_rounded_lines_ex(panel, round, 16, 3 * S, border);

    float cy = y + pad;
    if (comp_h > 0) {
        const float tx = x + pad;
        const float ty = cy + (row_h - fs) / 2;
        draw_text_ex(font, st.composition.c_str(), {tx, ty}, static_cast<float>(fs), spacing, Color::BLACK);
        const float uy = ty + fs + 2 * S;
        draw_rectangle(Rect{tx, uy, comp_w, 2 * S}, Color{90, 90, 90, 255});
        const size_t b0 = utf8_offset(st.composition, st.start);
        const float x0 = tx + measure(st.composition.substr(0, b0));
        if (st.length > 0) {
            const size_t b1 = utf8_offset(st.composition, st.start + st.length);
            const float x1 = tx + measure(st.composition.substr(0, b1));
            draw_rectangle(Rect{x0, uy - 1 * S, x1 - x0, 4 * S}, border);
        } else {
            draw_rectangle(Rect{x0, ty - 2 * S, 2 * S, fs + 4 * S}, Color::BLACK); // caret
        }
        cy += comp_h + sep;
        if (cand_h > 0) {
            draw_rectangle(Rect{x + pad, cy - sep / 2 - 1 * S, w - 2 * pad, 1 * S}, Color{200, 200, 200, 255});
        }
    }
    float cx = x + pad + pad;
    for (size_t i = 0; i < labels.size(); ++i) {
        const bool is_sel = static_cast<int>(i) == sel;
        const Rect cell =
            horizontal ? Rect{cx - pad / 2, cy, label_w[i] + pad, row_h} : Rect{x + pad, cy, w - 2 * pad, row_h};
        if (is_sel) {
            draw_rectangle_rounded(cell, 0.3f, 8, border);
        }
        const float tx = horizontal ? cx : x + 2 * pad;
        draw_text_ex(font, labels[i].c_str(), {tx, cy + (row_h - fs) / 2}, static_cast<float>(fs), spacing,
                     is_sel ? Color::WHITE : Color::BLACK);
        if (horizontal) {
            cx += label_w[i] + gap;
        } else {
            cy += row_h;
        }
    }
    if (horizontal && !labels.empty()) {
        cy += row_h;
    }
    if (!page_label.empty()) {
        const float pw = measure(page_label);
        draw_text_ex(font, page_label.c_str(), {x + w - pad - pw * 0.7f, cy + 2 * S}, fs * 0.7f, spacing,
                     Color{110, 110, 110, 255});
    }
}
