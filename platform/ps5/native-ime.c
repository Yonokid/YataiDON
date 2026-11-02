/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* Altered from the public PS5 SDL keyboard backend: IME only, with explicit
 * confirm/cancel delivery, checked lifecycle and locale-independent UTF-8.
 * No physical keyboard is required to open the system dialog.
 */
#include "native-ime.h"
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef enum SceImeDialogStatus
{
    SCE_IME_DIALOG_STATUS_NONE,
    SCE_IME_DIALOG_STATUS_RUNNING,
    SCE_IME_DIALOG_STATUS_FINISHED
} SceImeDialogStatus;

typedef int (*SceImeTextFilter)(wchar_t*, uint32_t*, const wchar_t*, uint32_t);

typedef struct SceImeDialogParam
{
    int userId;
    enum {
    SCE_IME_TYPE_DEFAULT,
    SCE_IME_TYPE_BASIC_LATIN,
    SCE_IME_TYPE_URL,
    SCE_IME_TYPE_MAIL,
    SCE_IME_TYPE_NUMBER
    } type;
    uint64_t supportedLanguages;
    enum {
    SCE_IME_ENTER_LABEL_DEFAULT,
    SCE_IME_ENTER_LABEL_SEND,
    SCE_IME_ENTER_LABEL_SEARCH,
    SCE_IME_ENTER_LABEL_GO,
    } enterLabel;
    enum {
    SCE_IME_INPUT_METHOD_DEFAULT
    } inputMethod;
    SceImeTextFilter filter;
    uint32_t option;
    uint32_t maxTextLength;
    wchar_t *inputTextBuffer;
    float posx;
    float posy;
    enum {
    SCE_IME_HALIGN_LEFT,
    SCE_IME_HALIGN_CENTER,
    SCE_IME_HALIGN_RIGHT
    } halign;
    enum {
    SCE_IME_VALIGN_TOP,
    SCE_IME_VALIGN_CENTER,
    SCE_IME_VALIGN_BOTTOM
    } valign;
    const wchar_t *placeholder;
    const wchar_t *title;
    int8_t reserved[16];
} SceImeDialogParam;

typedef struct SceImeDialogResult
{
    enum {
    SCE_IME_DIALOG_END_STATUS_OK,
    SCE_IME_DIALOG_END_STATUS_USER_CANCELED,
    SCE_IME_DIALOG_END_STATUS_ABORTED,
    } outcome;
    int8_t reserved[12];
} SceImeDialogResult;

_Static_assert(sizeof(SceImeDialogParam) == 96, "IME parameter ABI");
_Static_assert(sizeof(SceImeDialogResult) == 16, "IME result ABI");
extern int sceSysmoduleLoadModule(uint16_t);
extern int sceUserServiceGetForegroundUser(int*);
extern int sceImeDialogInit(const SceImeDialogParam*, void*);
extern int sceImeDialogGetResult(SceImeDialogResult*);
extern int sceImeDialogTerm(void);
extern void sceImeDialogAbort(void);
extern SceImeDialogStatus sceImeDialogGetStatus(void);

enum {
    IME_OPTION_FIXED_POSITION = 0x40,
    IME_OPTION_DISABLE_POSITION_ADJUSTMENT = 0x800
};

static int active;
static int replacement_ready;
static wchar_t input_text[256];
static const wchar_t title[] = L"Keyboard";

static void to_utf8(char* out, size_t capacity) {
    size_t written = 0;
    for (size_t i = 0; i < 255 && input_text[i]; ++i) {
        uint32_t cp = (uint32_t)input_text[i];
        if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < 255 &&
            input_text[i + 1] >= 0xdc00 && input_text[i + 1] <= 0xdfff) {
            cp = 0x10000 + ((cp - 0xd800) << 10) + ((uint32_t)input_text[++i] - 0xdc00);
        } else if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            cp = 0xfffd;
        }
        unsigned n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (written + n >= capacity) break;
        if (n == 1) out[written++] = (char)cp;
        else {
            out[written++] = (char)((n == 2 ? 0xc0 : n == 3 ? 0xe0 : 0xf0) | (cp >> (6 * (n - 1))));
            for (unsigned j = n - 1; j > 0; --j)
                out[written++] = (char)(0x80 | ((cp >> (6 * (j - 1))) & 0x3f));
        }
    }
    if (capacity) out[written] = 0;
}

int yataidon_ime_active(void) { return active; }

// The IME returns the complete edited field, rather than an insertion.
int yataidon_ime_take_replacement(void) {
    int ready = replacement_ready;
    replacement_ready = 0;
    return ready;
}

static void prefill(const char* text) {
    memset(input_text, 0, sizeof(input_text));
    if (!text) return;
    const unsigned char* bytes = (const unsigned char*)text;
    size_t length = strlen(text), written = 0;
    for (size_t i = 0; i < length && written < 255;) {
        uint32_t cp = bytes[i++];
        unsigned extra = cp < 0x80 ? 0 : cp >= 0xc2 && cp <= 0xdf ? 1 :
                         cp >= 0xe0 && cp <= 0xef ? 2 : cp >= 0xf0 && cp <= 0xf4 ? 3 : 4;
        if (extra == 4) cp = 0xfffd;
        else if (extra) {
            uint32_t value = cp & ((1u << (6 - extra)) - 1);
            unsigned n = 0;
            while (n < extra && i < length && (bytes[i] & 0xc0) == 0x80) {
                value = (value << 6) | (bytes[i++] & 0x3f);
                ++n;
            }
            cp = n != extra || value < (extra == 1 ? 0x80u : extra == 2 ? 0x800u : 0x10000u) ||
                 value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ? 0xfffd : value;
        }
#if WCHAR_MAX <= 0xffff
        if (cp > 0xffff) {
            if (written + 2 > 255) break;
            input_text[written++] = (wchar_t)(0xd800 + ((cp - 0x10000) >> 10));
            input_text[written++] = (wchar_t)(0xdc00 + ((cp - 0x10000) & 0x3ff));
            continue;
        }
#endif
        input_text[written++] = (wchar_t)cp;
    }
}

int yataidon_ime_open(const char* initial_text) {
    if (active) return 0;
    int loaded = sceSysmoduleLoadModule(0x0096); // public IME_DIALOG module
    void* volatile imports[] = {(void*)sceImeDialogInit, (void*)sceImeDialogGetStatus,
        (void*)sceImeDialogGetResult, (void*)sceImeDialogTerm, (void*)sceImeDialogAbort};
    for (size_t i = 0; i < sizeof(imports) / sizeof(imports[0]); ++i) {
        if (!imports[i]) {
            fprintf(stderr, "[yataidon-ime] unavailable import; module=%08x\n", (unsigned)loaded);
            return -1;
        }
    }
    SceImeDialogParam param = {0};
    int rc = sceUserServiceGetForegroundUser(&param.userId);
    if (rc != 0) return rc;
    replacement_ready = 0;
    prefill(initial_text);
    param.enterLabel = SCE_IME_ENTER_LABEL_DEFAULT;
    param.maxTextLength = 255;
    param.inputTextBuffer = input_text;
    param.title = title;
    // IME uses a 1920x1080 reference canvas unless over-2K coordinates are enabled.
    // Center the panel on that canvas; the system scales it to the display output.
    param.option = IME_OPTION_FIXED_POSITION | IME_OPTION_DISABLE_POSITION_ADJUSTMENT;
    param.posx = 960.0f;
    param.posy = 540.0f;
    param.halign = SCE_IME_HALIGN_CENTER;
    param.valign = SCE_IME_VALIGN_CENTER;
    rc = sceImeDialogInit(&param, NULL);
    fprintf(stderr, "[yataidon-ime] centered-v2 open result=%08x option=%08x anchor=%.0f,%.0f align=%d,%d\n",
        (unsigned)rc, param.option, param.posx, param.posy, param.halign, param.valign);
    if (rc == 0) active = 1;
    return rc;
}

void yataidon_ime_close(void) {
    if (!active) return;
    if (sceImeDialogGetStatus() == SCE_IME_DIALOG_STATUS_RUNNING) sceImeDialogAbort();
    int rc = sceImeDialogTerm();
    if (rc != 0) fprintf(stderr, "[yataidon-ime] term result=%08x\n", (unsigned)rc);
    active = 0;
}

int yataidon_ime_poll(char* text, size_t capacity) {
    if (!active) return YATAIDON_IME_PENDING;
    SceImeDialogStatus status = sceImeDialogGetStatus();
    if (status == SCE_IME_DIALOG_STATUS_RUNNING) return YATAIDON_IME_PENDING;
    SceImeDialogResult result = {0};
    int confirmed = status == SCE_IME_DIALOG_STATUS_FINISHED &&
                    sceImeDialogGetResult(&result) == 0 && result.outcome == SCE_IME_DIALOG_END_STATUS_OK;
    if (confirmed) {
        to_utf8(text, capacity);
        replacement_ready = 1;
    }
    yataidon_ime_close();
    fprintf(stderr, "[yataidon-ime] completed confirmed=%d\n", confirmed);
    return confirmed ? YATAIDON_IME_CONFIRMED : YATAIDON_IME_CANCELED;
}
