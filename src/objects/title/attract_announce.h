#pragma once

#include <string>

#include "../../libs/file/script.h"

// Attract-mode campaign badge, or the tournament kit icon (Scripts/title/attract_announce.lua)
class AttractAnnounce : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;

public:
    AttractAnnounce() = default;
    explicit AttractAnnounce(const std::string& mode);
    void update(double current_ms);
    void draw(float fade = 1.0f);
};
