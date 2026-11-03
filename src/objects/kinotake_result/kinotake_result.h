#pragma once

#include "../../libs/file/script.h"
#include "../../libs/network/network.h"

class KinotakeResult : public LuaScript {
public:
    explicit KinotakeResult(const KinotakeTally& tally);
    bool unavailable() const { return !fn_update.valid(); }
    void update(double current_ms);
    bool skip();
    void draw();
    bool is_done();
    std::pair<float, float> nameplate(int seat);

private:
    sol::protected_function fn_update;
    sol::protected_function fn_skip;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_nameplate;
};
