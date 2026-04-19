#pragma once
#include "box_base.h"
#include "../../libs/text.h"
#include "../../libs/audio.h"
#include "../../libs/scores.h"

class FolderBox : public BaseBox {
public:
    int tja_count;
    std::map<int, Crown> crown;
    bool entered = false;
    std::unique_ptr<FadeAnimation> enter_fade;
    std::optional<ray::Texture> box_texture;

    std::unique_ptr<OutlinedText> hori_name;
    std::unique_ptr<OutlinedText> tja_count_text;

    FolderBox(const fs::path& path, const BoxDef& box_def, int tja_count, std::map<std::pair<std::string, std::string>, fs::path>& song_files);
    ~FolderBox() override;

    void load_text() override;
    void update(double current_time) override;

    void enter_box() override;
    void exit_box() override;

protected:
    void draw_open_bg(float fade);
    void draw_open_fg(float fade);
    void draw_closed() override;
    void draw_open() override;
};
