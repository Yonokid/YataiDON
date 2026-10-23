#pragma once

#include "ray.h"

// In-game IME UI: the game draws the IME composition string and candidate list itself (SDL is told
// so with SDL_HINT_IME_IMPLEMENTED_UI), since the OS windows are hidden behind a fullscreen or
// borderless game window. Desktop SDL3 only; elsewhere these are no-ops and nothing is drawn.
// The overlay is only up while a text field is edited (poll_text_edit / sync_text_input).

void ime_ui_set_hint();                        // before InitWindow (SDL reads it when text input first starts)
void ime_ui_install();                         // after InitWindow: watches the SDL text editing events
void ime_ui_text_input_changed(bool active);   // sync_text_input started/stopped text input
void ime_ui_set_field(ray::Rectangle rect);    // virtual-screen rect of the field being edited, this frame
void ime_ui_draw(const ray::Camera2D& camera); // overlay, inside BeginMode2D(camera)
