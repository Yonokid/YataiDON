#pragma once
#include "box_base.h"

class BackBox : public BaseBox {
public:
    static constexpr ray::Color COLOR = ray::Color(170, 115, 35);

    BackBox(const fs::path& path, const std::optional<ray::Color> back_color,
            const std::optional<ray::Color> fore_color, TextureIndex texture_index);

    void expand_box() override;

protected:
    void draw_closed(float outer_fade_override) override;
    void draw_open(std::optional<float> fade_override) override;
};
