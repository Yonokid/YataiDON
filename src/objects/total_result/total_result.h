#pragma once

#include <unordered_map>

#include "../../libs/file/filesystem.h"
#include "../../libs/file/script.h"
#include "../../libs/gfx/texture.h"

struct RewardArt {
    std::string name;
    fs::path icon;
};

RewardArt reward_art(int type, int id);

class TotalResult : public LuaScript {
public:
    TotalResult();
    ~TotalResult() override;
    bool unavailable() const { return !fn_update.valid(); }
    void update(double current_ms);
    bool skip();
    void draw();
    bool is_done();
    float nameplate_fade();

private:
    sol::protected_function fn_update;
    sol::protected_function fn_skip;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_nameplate;
    std::unordered_map<int, Texture> icons;
};
