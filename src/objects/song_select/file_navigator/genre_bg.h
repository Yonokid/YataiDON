#pragma once

#include "box_folder.h"

class GenreBG {
private:
    ray::Shader shader;
    bool shader_loaded = false;
    std::unique_ptr<OutlinedText> name;
    TextureIndex texture_index;
    void draw_anim(FolderBox* box);
    void draw_exit_anim(float start_position, float end_position, FolderBox* folder);

    MoveAnimation* stretch;
    TextureResizeAnimation* scale;
    MoveAnimation* move;
    FadeAnimation* fade;
    MoveAnimation* move_left;
    MoveAnimation* move_right;
public:
    GenreBG(std::string& text_name, std::optional<ray::Color> color, TextureIndex texture_index, float distance);
    void update(double current_ms, FolderBox* box);
    void draw(float start_position, float end_position, FolderBox* folder);
    void exit(float left_position, float right_position, FolderBox* center_box);
    void fade_out();
    void fade_in();

    bool is_finished();
    bool is_complete();
};
