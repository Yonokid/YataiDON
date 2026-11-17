#pragma once

#include "../../../libs/gfx/color_utils.h"
#include "box_folder.h"
#include "gradient_textures.h"

class GenreBG {
public:
    enum class Phase {
        POP_IN,
        WAIT,
        OPENING,
        OPEN,
        CLOSING,
        SHUT,
        POP_OUT,
        DONE
    };

    struct Span {
        float left;
        float right;
    };

    GenreBG(const std::string& text_name, std::vector<Color> colors, int texture_index);

    void update(double current_ms);
    void set_edges(Span closed, Span live, float pitch);

    void open_sides();
    void close_sides();
    void pop_out();
    void settle();
    void fade_out();
    void fade_in();

    Phase phase() const { return phase_; }
    bool popped_in() const { return phase_ != Phase::POP_IN; }
    bool sides_moving() const { return phase_ == Phase::OPENING || phase_ == Phase::CLOSING; }
    bool shut() const { return phase_ == Phase::SHUT; }
    bool is_done() const { return phase_ == Phase::DONE; }

    float alpha() const;
    float title_alpha() const { return dim ? static_cast<float>(dim->attribute) : 1.0f; }
    int texture_frame() const { return texture_index; }
    OutlinedText* name_text() const { return name.get(); }
    bool has_recolor() const { return shader_loaded && !gradients.active(); }
    bool has_gradient() const { return gradients.active(); }
    std::shared_ptr<TextureObject> gradient_texture(const std::string& name) { return gradients.get(name); }
    void begin_recolor() {
        if (shader_loaded) {
            begin_shader_mode(shader);
        }
    }
    void end_recolor() {
        if (shader_loaded) {
            end_shader_mode();
        }
    }

    float left = 0.0f;
    float right = 0.0f;
    float center = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    std::optional<Span> wrap;
    bool show_tab = true;

private:
    double side_ms(float distance, float pitch) const;
    void enter(Phase phase);

    Phase phase_ = Phase::POP_IN;
    double now_ms = 0.0;
    double phase_ms = -1.0;
    double duration_scale = 1.0;
    float pop_alpha = 0.0f;
    Span close_from{};
    Span closed_{};
    float reach_ = 0.0f;
    std::unique_ptr<FadeAnimation> dim;

    Shader shader;
    std::shared_ptr<Shader> shader_owner;
    bool shader_loaded = false;
    std::unique_ptr<OutlinedText> name;
    int texture_index;
    GradientTextures gradients;
};
