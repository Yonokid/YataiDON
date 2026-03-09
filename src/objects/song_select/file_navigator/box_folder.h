#pragma once
#include "box_base.h"
#include "../../libs/text.h"
#include "../../libs/audio.h"

class FolderBox : public BaseBox {
public:
    int tja_count;
    std::map<int, Crown> crown;
    GenreIndex genre_index;

    std::unique_ptr<OutlinedText> hori_name;
    std::unique_ptr<OutlinedText> tja_count_text;

    FolderBox(const fs::path& path,
              const std::optional<ray::Color>& back_color,
              const std::optional<ray::Color>& fore_color,
              TextureIndex texture_index,
              GenreIndex genre_index,
              const std::string& text_name,
              int tja_count = 0);

    void load_text() override;
    void update(double current_time) override;

protected:
    void draw_closed() override;
    void draw_open() override;
};
