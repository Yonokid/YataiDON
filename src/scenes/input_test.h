#pragma once

#include <algorithm>

#include "../libs/screen.h"
#include "../objects/game/drum_hit_effect.h"

class InputTestDrumEffect : public DrumHitEffect {
private:
    float x_offset;
    float y_offset;
    TextureObject* t_large_drum = nullptr;
    TextureObject* t_drum = nullptr;

public:
    InputTestDrumEffect(DrumType type, Side side, float x_offset, float y_offset)
        : DrumHitEffect(type, side), x_offset(x_offset), y_offset(y_offset) {
        t_large_drum = tex.get_texture("practice/large_drum");
        if (type == DrumType::DON) {
            t_drum = tex.get_texture("practice/large_drum_don");
        } else if (side == Side::LEFT) {
            t_drum = tex.get_texture("practice/large_drum_kat_l");
        } else {
            t_drum = tex.get_texture("practice/large_drum_kat_r");
        }
    }

    void draw(float) override {
        if (type == DrumType::DON && t_large_drum) {
            // The skin ships a single full-face don flash, so clip it to the
            // half that was actually hit - otherwise left and right don look
            // identical on the input test.
            TextureObject& drum = *t_large_drum;
            float left = x_offset + drum.x[0];
            float centre = left + drum.x2[0] / 2.0f;
            float right = left + drum.x2[0];
            int sx = virtual_to_screen_x(side == Side::LEFT ? left : centre);
            int ex = virtual_to_screen_x(side == Side::LEFT ? centre : right);
            begin_scissor_mode(sx, 0, std::max(0, ex - sx), get_render_height());
            tex.draw_texture(t_drum, {.x = x_offset, .y = y_offset, .fade = fade->attribute});
            end_scissor_mode();
        } else {
            tex.draw_texture(t_drum, {.x = x_offset, .y = y_offset, .fade = fade->attribute});
        }
    }
};

class InputTestScreen : public Screen {
private:
    std::vector<std::unique_ptr<DrumHitEffect>> hit_effects;
    float drum_x_offset = 0;
    float drum_y_offset = 0;
    TextureObject* t_background = nullptr;
    TextureObject* t_large_drum = nullptr;

public:
    TouchInput touch_input() const override { return TouchInput::DRUM; }
    InputTestScreen() : Screen("input_test") {}

    void on_screen_start() override;
    std::optional<Screens> update() override;
    void draw() override;
};
