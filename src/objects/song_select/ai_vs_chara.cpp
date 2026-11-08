#include "ai_vs_chara.h"

#include <array>
#include <memory>

#include "../../libs/file/scores.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/global_data.h"
#include "../game/ai_match.h"
#include "../global/chara_3d.h"

namespace {
    std::array<std::unique_ptr<Chara3D>, 2> charas;

    void load() {
        const PlayerNum seat = global_data.player_num;
        if (!charas[0]) {
            const auto pd = scores_manager.get_player_data(get_player_id(seat));
            charas[0] = make_chara_from_player_data(pd ? &*pd : nullptr, false, true);
            if (pd) {
                charas[0]->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
                charas[0]->apply_face(pd->chara_face_index);
            } else {
                charas[0]->set_don_colors(chara_default_color_1(seat), chara_default_color_2(seat),
                                          {249, 240, 225, 255});
            }
            charas[0]->set_puchi(0); // the window shows the Don alone
            charas[0]->set_anim(AnimIndex::DON_NORMAL);
        }
        if (!charas[1]) {
            PlayerData ai;
            ai.chara_cos_index = AI_COSTUME;
            charas[1] = make_chara_from_player_data(&ai, false, true);
            charas[1]->set_anim(AnimIndex::DON_NORMAL);
        }
    }

    void draw(int side, float x, float y, float scale, float rot_y) {
        load();
        Chara3D& chara = *charas[side == 0 ? 0 : 1];
        const Chara3DConfig& cfg = tex.chara_3d_config;
        chara.update(get_current_ms());
        chara.set_rotation(cfg.rot_x, rot_y, cfg.rot_z);
        chara.draw(x, y, scale);
    }
} // namespace

void ai_vs_chara_release() {
    charas[0].reset();
    charas[1].reset();
}

void ai_vs_chara_bind(sol::table tex_table) {
    tex_table.set_function("ai_vs_chara_load", [] { load(); });
    tex_table.set_function("ai_vs_chara_draw",
                           [](int side, float x, float y, sol::optional<float> scale, sol::optional<float> rot) {
                               draw(side, x, y, scale.value_or(1.0f), rot.value_or(0.0f));
                           });
    tex_table.set_function("ai_vs_chara_release", [] { ai_vs_chara_release(); });
}
