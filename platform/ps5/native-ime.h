/* Native PS5 keyboard lifecycle, polled on the video thread. */
#pragma once
#include <stddef.h>
enum { YATAIDON_IME_PENDING, YATAIDON_IME_CONFIRMED, YATAIDON_IME_CANCELED };
#ifdef __cplusplus
extern "C" {
#endif
int yataidon_ime_open(const char* initial_text);
int yataidon_ime_take_replacement(void);
int yataidon_ime_active(void);
int yataidon_ime_poll(char* text, size_t capacity);
void yataidon_ime_close(void);

#ifdef __cplusplus
}
#endif
