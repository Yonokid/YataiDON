#pragma once

#include <array>

#include "../gfx.h"

// Per-string lookup: no retained font pointers or lifetime-sensitive global cache.
class GlyphLookup {
    const Font& font;
    std::array<int, 128> ascii;
    bool active = false;

public:
    GlyphLookup(const Font& font, const char* text) : font(font) {
        if (!text || font.baseSize <= 0 || font.glyphCount < 128 || !font.recs || !font.glyphs) {
            return;
        }
        std::array<bool, 128> needed{};
        unsigned length = 0, remaining = 0;
        for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
            if (*p >= needed.size()) {
                continue;
            }
            ++length;
            if (*p == '\n') {
                continue;
            }
            if (!needed[*p]) {
                needed[*p] = true;
                ++remaining;
            }
        }
        if (length < 8) {
            return;
        }
        active = true;
        ascii.fill(-1);
        for (int i = 0; i < font.glyphCount && remaining; ++i) {
            const int cp = font.glyphs[i].value;
            if (cp >= 0 && cp < static_cast<int>(ascii.size()) && needed[cp] && ascii[cp] < 0) {
                ascii[cp] = i;
                --remaining;
            }
        }
    }

    int index(int codepoint) const {
        if (active && codepoint >= 0 && codepoint < static_cast<int>(ascii.size()) && ascii[codepoint] >= 0) {
            return ascii[codepoint];
        }
        return get_glyph_index(font, codepoint);
    }
};
