#include "judgment.h"

#include <stdexcept>

#include "../../libs/global_data.h"

namespace {
    // Japanese is the base art (judge_good.png); other languages add a suffix, and the engine's
    // language_variants order would try "_en" before the base for "ja", so ja asks for the bare name
    std::string judge_text_name(const char* base) {
        const std::string lang = global_data.config ? global_data.config->general.language : "";
        return std::string("hit_effect/") + base + ((lang.empty() || lang == "ja") ? "" : "_" + lang);
    }
} // namespace

Judgment::Judgment(Judgments type, bool big) : type(type) {

    big_outer = big && type != Judgments::BAD && tex.has_animation(68) && tex.has_animation(69);
    fade_animation_1 = dynamic_cast<FadeAnimation*>(tex.get_animation(big_outer ? 69 : 27, true));
    fade_animation_2 = dynamic_cast<FadeAnimation*>(tex.get_animation(28, true));
    // 77 (the hit effect's own fade) is newer than many skins' game/animation.json; without it the
    // effect fades with the text (28) as it did before
    if (tex.has_animation(77)) {
        fade_animation_3 = dynamic_cast<FadeAnimation*>(tex.get_animation(77, true));
    }
    move_animation = dynamic_cast<MoveAnimation*>(tex.get_animation(29, true));
    texture_animation = dynamic_cast<TextureChangeAnimation*>(tex.get_animation(big_outer ? 68 : 30, true));
    if (!fade_animation_1 || !fade_animation_2 || !move_animation || !texture_animation) {
        throw std::runtime_error("Judgment: animation 27/28/29/30 has an unexpected type");
    }

    move_animation->start();
    fade_animation_2->start();
    fade_animation_1->start();
    if (fade_animation_3) {
        fade_animation_3->start();
    }
    texture_animation->start();

    if (big_outer && tex.has_animation(70)) {
        outer_scale = dynamic_cast<TextureResizeAnimation*>(tex.get_animation(70, true));
        if (outer_scale) {
            outer_scale->start();
        }
    }
    if (big_outer && tex.has_animation(74) && type != Judgments::BAD) {
        const char* ray_name = type == Judgments::GOOD ? "hit_effect/outer_effect_good" : "hit_effect/outer_effect_ok";
        if (tex.has_texture(ray_name)) {
            // a skin may use these ids for animations of its own: no rings unless the types fit
            ray_scale =
                tex.has_animation(71) ? dynamic_cast<TextureResizeAnimation*>(tex.get_animation(71, true)) : nullptr;
            ray_fade = tex.has_animation(72) ? dynamic_cast<FadeAnimation*>(tex.get_animation(72, true)) : nullptr;
            if (ray_scale && ray_fade) {
                t_ray = tex.get_texture(ray_name);
                ray_scale->start();
                ray_fade->start();
                if (type == Judgments::GOOD && tex.has_animation(73)) {
                    ray2_scale = dynamic_cast<TextureResizeAnimation*>(tex.get_animation(73, true));
                    ray2_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(74, true));
                    if (ray2_scale && ray2_fade) {
                        ray2_scale->start();
                        ray2_fade->start();
                    } else {
                        ray2_scale = nullptr;
                        ray2_fade = nullptr;
                    }
                }
            } else {
                ray_scale = nullptr;
                ray_fade = nullptr;
            }
        }
    }

    if (type == Judgments::GOOD) {
        t_effect = tex.get_texture(big ? "hit_effect/hit_effect_good_big" : "hit_effect/hit_effect_good");
        t_outer_effect = tex.get_texture(big ? "hit_effect/outer_good_big" : "hit_effect/outer_good");
        t_text = tex.get_texture(judge_text_name("judge_good"));
    } else if (type == Judgments::OK) {
        t_effect = tex.get_texture(big ? "hit_effect/hit_effect_ok_big" : "hit_effect/hit_effect_ok");
        t_outer_effect = tex.get_texture(big ? "hit_effect/outer_ok_big" : "hit_effect/outer_ok");
        t_text = tex.get_texture(judge_text_name("judge_ok"));
    } else if (type == Judgments::BAD) {
        t_text = tex.get_texture(judge_text_name("judge_bad"));
    }
}

void Judgment::update(double current_ms) {
    BaseAnimation* animations[] = {fade_animation_1,  fade_animation_2, fade_animation_3, move_animation,
                                   texture_animation, outer_scale,      ray_scale,        ray_fade,
                                   ray2_scale,        ray2_fade};

    for (auto& animation : animations) {
        if (animation) {
            animation->update(current_ms);
        }
    }
}

void Judgment::draw_effect(float judge_x, float judge_y) {
    float fade = (fade_animation_3 ? fade_animation_3 : fade_animation_2)->attribute;
    tex.draw_texture(t_effect, {.x = judge_x, .y = judge_y, .fade = fade});
}

void Judgment::draw_outer_effect(float judge_x, float judge_y) {
    int index = static_cast<int>(texture_animation->attribute);
    float hit_fade = fade_animation_1->attribute;
    float scale = outer_scale ? outer_scale->attribute : 1.0f;
    tex.draw_texture(t_outer_effect, {.frame = index,
                                      .scale = scale,
                                      .center = true,
                                      .x = judge_x,
                                      .y = judge_y,
                                      .fade = hit_fade,
                                      .blend = blend_mode::ADDITIVE});
}

void Judgment::draw_ray_effect(float judge_x, float judge_y) {
    if (!t_ray) {
        return;
    }
    tex.draw_texture(t_ray, {.scale = static_cast<float>(ray_scale->attribute),
                             .center = true,
                             .x = judge_x,
                             .y = judge_y,
                             .fade = ray_fade->attribute,
                             .blend = blend_mode::ADDITIVE});
    if (ray2_scale) {
        tex.draw_texture(t_ray, {.scale = static_cast<float>(ray2_scale->attribute),
                                 .center = true,
                                 .x = judge_x,
                                 .y = judge_y,
                                 .fade = ray2_fade->attribute,
                                 .blend = blend_mode::ADDITIVE});
    }
}

void Judgment::draw_text(float judge_x, float judge_y) {
    float y = move_animation->attribute;
    int index = static_cast<int>(texture_animation->attribute);
    float fade = fade_animation_2->attribute;

    if (type == Judgments::GOOD) {
        tex.draw_texture(t_text, {.frame = index, .x = judge_x, .y = y + judge_y, .fade = fade});
    } else {
        tex.draw_texture(t_text, {.x = judge_x, .y = y + judge_y, .fade = fade});
    }
}

bool Judgment::is_finished() const {
    return fade_animation_2->is_finished && (!fade_animation_3 || fade_animation_3->is_finished);
}
