#include "encoding.h"

#include "../platform/platform_windows.h"

#if defined(YATAIDON_HAVE_ICONV)
#include <iconv.h>
#endif

bool is_valid_utf8(std::string_view text) {
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            i++;
            continue;
        }
        int extra;
        unsigned int cp;
        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            cp = c & 0x07;
        } else {
            return false;
        }
        if (i + extra >= n) {
            return false;
        }
        for (int k = 1; k <= extra; k++) {
            const unsigned char cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        // overlong forms, surrogates and values past U+10FFFF are not UTF-8
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

std::string utf8_from_utf8_or_sjis(std::string_view text) {
    if (is_valid_utf8(text)) {
        return std::string(text);
    }
#if defined(_WIN32)
    return win32_utf8_from_sjis(text);
#elif defined(YATAIDON_HAVE_ICONV)
    iconv_t cd = iconv_open("UTF-8", "CP932");
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        return std::string(text);
    }
    std::string out(text.size() * 3 + 4, '\0');
    char* in_ptr = const_cast<char*>(text.data());
    size_t in_left = text.size();
    char* out_ptr = out.data();
    size_t out_left = out.size();
    while (in_left > 0) {
        if (iconv(cd, &in_ptr, &in_left, &out_ptr, &out_left) != static_cast<size_t>(-1)) {
            break;
        }
        // a byte CP932 does not map: keep it as '?' and go on
        if (out_left == 0) {
            break;
        }
        *out_ptr++ = '?';
        out_left--;
        in_ptr++;
        in_left--;
    }
    iconv_close(cd);
    out.resize(out.size() - out_left);
    return out;
#else
    return std::string(text);
#endif
}
