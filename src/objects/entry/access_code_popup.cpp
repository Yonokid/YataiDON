#include "access_code_popup.h"

#include "../../libs/gfx/qr_texture.h"
#include "../../libs/global_data.h"

static constexpr float QR_SIZE_BASE = 260.0f;
static constexpr float CARD_GAP_BASE = 60.0f;
static constexpr float PANEL_PAD_BASE = 40.0f;
static constexpr float TITLE_FONT_BASE = 40.0f;
static constexpr float TEXT_FONT_BASE = 30.0f;
static constexpr float TEXT_GAP_BASE = 12.0f;

static std::unique_ptr<OutlinedText> make_text(const std::string& str, float base_size) {
    return std::make_unique<OutlinedText>(str, static_cast<int>(base_size * tex.screen_scale), Color::WHITE,
                                          Color::BLACK, false, 4, -4);
}

AccessCodePopup::AccessCodePopup(const std::vector<Entry>& entries) : title(make_text("Access Code", TITLE_FONT_BASE)) {
    for (const auto& entry : entries) {
        Card card{.frame = load_frame(entry.is_2p), .qr = {}, .label = nullptr, .code = nullptr};
        card.qr = make_qr_texture(access_code_login_url(entry.code));
        if (!entry.label.empty()) {
            card.label = make_text(entry.label, TEXT_FONT_BASE);
        }
        card.code = make_text(entry.code, TEXT_FONT_BASE);
        cards.push_back(std::move(card));
    }
}

AccessCodePopup::Frame AccessCodePopup::load_frame(bool is_2p) {
    const std::string suffix = is_2p ? "_2p" : "_1p";
    const auto get = [&suffix](const char* name) {
        return tex.get_texture(std::string("costume_select/") + name + suffix);
    };
    return {get("box_top_left"), get("box_top_right"), get("box_bottom_left"), get("box_bottom_right"), get("box_top"),
            get("box_bottom"),   get("box_left"),      get("box_right"),       get("box_center")};
}

// The costume menu box is a 9-slice authored at one size; extra_w / extra_h stretch it to w x h.
void AccessCodePopup::draw_frame(const Frame& f, float x, float y, float w, float h) {
    const float extra_w = w - (f.top_right->x[0] + f.top_right->width);
    const float extra_h = h - (f.bottom_left->y[0] + f.bottom_left->height);
    const float ox = x - f.top_left->x[0];
    const float oy = y - f.top_left->y[0];
    tex.draw_texture(f.top_left, {.x = ox, .y = oy});
    tex.draw_texture(f.top_right, {.x = ox + extra_w, .y = oy});
    tex.draw_texture(f.bottom_left, {.x = ox, .y = oy + extra_h});
    tex.draw_texture(f.bottom_right, {.x = ox + extra_w, .y = oy + extra_h});
    tex.draw_texture(f.top, {.x = ox, .y = oy, .x2 = extra_w});
    tex.draw_texture(f.bottom, {.x = ox, .y = oy + extra_h, .x2 = extra_w});
    tex.draw_texture(f.left, {.x = ox, .y = oy, .y2 = extra_h});
    tex.draw_texture(f.right, {.x = ox + extra_w, .y = oy, .y2 = extra_h});
    tex.draw_texture(f.center, {.x = ox, .y = oy, .x2 = extra_w, .y2 = extra_h});
}

AccessCodePopup::~AccessCodePopup() {
    for (auto& card : cards) {
        unload_texture(card.qr);
    }
}

void AccessCodePopup::draw() {
    const float s = tex.screen_scale;
    const float qr = QR_SIZE_BASE * s;
    const float gap = CARD_GAP_BASE * s;
    const float pad = PANEL_PAD_BASE * s;
    const float text_gap = TEXT_GAP_BASE * s;
    const float label_h = cards.empty() || !cards[0].label ? 0.0f : cards[0].label->height + text_gap;
    const float code_h = cards.empty() ? 0.0f : cards[0].code->height;

    float cards_w = 0.0f;
    for (const auto& card : cards) {
        cards_w += std::max(qr, card.code->width) + gap;
    }
    cards_w -= gap;
    const float panel_w = std::max(cards_w, title->width) + (pad * 2.0f);
    const float panel_h = pad + title->height + text_gap + label_h + qr + text_gap + code_h + pad;
    const float panel_x = (tex.screen_width - panel_w) / 2.0f;
    const float panel_y = (tex.screen_height - panel_h) / 2.0f;

    draw_rectangle(0, 0, tex.screen_width, tex.screen_height, fade(Color::BLACK, 0.6f));
    const float half_w = panel_w / static_cast<float>(cards.size());
    for (size_t i = 0; i < cards.size(); i++) {
        draw_frame(cards[i].frame, panel_x + (half_w * static_cast<float>(i)), panel_y, half_w, panel_h);
    }

    title->draw({.x = panel_x + (panel_w - title->width) / 2.0f, .y = panel_y + pad});

    const float top = panel_y + pad + title->height + text_gap;
    for (size_t i = 0; i < cards.size(); i++) {
        const auto& card = cards[i];
        const float col_w = std::max(qr, card.code->width);
        const float x = panel_x + (half_w * (static_cast<float>(i) + 0.5f)) - (col_w / 2.0f);
        if (card.label) {
            card.label->draw({.x = x + (col_w - card.label->width) / 2.0f, .y = top});
        }
        const float qr_y = top + label_h;
        const auto src = static_cast<float>(card.qr.width);
        draw_texture_pro(card.qr, {0, 0, src, src}, {x + (col_w - qr) / 2.0f, qr_y, qr, qr}, {0, 0}, 0.0f,
                         Color::WHITE);
        card.code->draw({.x = x + (col_w - card.code->width) / 2.0f, .y = qr_y + qr + text_gap});
    }
}
