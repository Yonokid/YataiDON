#pragma once

#include "../../libs/file/script.h"
#include "../../libs/network/network.h"

class BattleSelectInfo : public LuaScript {
    sol::protected_function fn_draw;

public:
    BattleSelectInfo(const BattleStatus& status, int stage, int player);
    void draw(double ms);
};
