#pragma once

#include "../../../libs/global_data.h"
#include "box_base.h"

class FolderBox : public BaseBox {
public:
    int tja_count{0};
    bool is_osu_folder = false;
    std::map<int, Crown> crown;
    std::map<int, Crown> crown_p2;
    bool entered = false;
    bool genre_voice_started = false;
    std::unique_ptr<FadeAnimation> enter_fade;
    std::optional<Texture> box_texture;

    FolderBox(const fs::path& path, const BoxDef& box_def);
    ~FolderBox() override;

    void load_text() override;
    void update(double current_time) override;

    void enter_box() override;
    void exit_box() override;

    const char* lua_kind() const override { return "folder"; }
};
