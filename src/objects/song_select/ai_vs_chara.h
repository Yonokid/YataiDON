#pragma once

#include <sol/sol.hpp>

// The AI battle's loading VS window shows both 3D Dons. It is a Lua transition that runs over the song select and the
// game, so the two models are kept here, outside either screen:
//   tex.ai_vs_chara_load()                              loads them ahead of their first frame
//   tex.ai_vs_chara_draw(side, x, y, scale, rot_y)      side 0 = the player, 1 = the AI; feet at x, y
//   tex.ai_vs_chara_release()                           frees them
void ai_vs_chara_bind(sol::table tex_table);
void ai_vs_chara_release();
