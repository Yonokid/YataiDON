#pragma once

extern "C" void yataidon_switch_show_keyboard();
extern "C" int yataidon_switch_keyboard_shown();
extern "C" void yataidon_switch_cancel_keyboard_result();
bool switch_take_text_replacement();
void switch_poll_keyboard();
