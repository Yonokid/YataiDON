#pragma once

#include "../../../libs/gfx/color_utils.h"
#include "box_folder.h"

class GenreBG {
public:
    GenreBG(const std::string& text_name, std::optional<Color> color, TextureIndex texture_index, float distance);

    void update(double current_ms, FolderBox* box);
    void exit(float left_position, float right_position, FolderBox* center_box);
    void fade_out();
    void fade_in();
    bool is_finished();
    bool is_complete();

    int texture_frame() const { return static_cast<int>(texture_index); }
    OutlinedText* name_text() const { return name.get(); }
    bool has_recolor() const { return shader_loaded; }
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

    std::unique_ptr<MoveAnimation> stretch;
    std::unique_ptr<TextureResizeAnimation> scale;
    std::unique_ptr<MoveAnimation> move;
    std::unique_ptr<FadeAnimation> fade;
    std::unique_ptr<MoveAnimation> move_left;
    std::unique_ptr<MoveAnimation> move_right;

private:
    Shader shader;
    std::shared_ptr<Shader> shader_owner;
    bool shader_loaded = false;
    std::unique_ptr<OutlinedText> name;
    TextureIndex texture_index;
};
