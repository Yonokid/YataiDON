#pragma once

#include <functional>
#include <unordered_map>

#include "../../libs/file/script.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/network/network.h"

class RewardShop : public LuaScript {
public:
    enum Action {
        LEFT = 1,
        RIGHT = 2,
        DECIDE = 3
    };
    struct Hooks {
        std::function<void(int seat, int type, int id)> buy;
        std::function<void(int seat, int type, int id)> preview;
        std::function<void(int seat, float x, float y)> draw_chara;
    };

    RewardShop(bool from_time, const bool joined[2], Hooks hooks);
    ~RewardShop() override;
    bool unavailable() const { return !fn_update.valid(); }
    void update(double current_ms);
    void input(int seat, Action action);
    void on_buy(const ShopBuyResult& result);
    void draw();
    bool is_done();
    bool wants_input(int seat);
    bool in_shop();
    void time_up();

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_on_buy;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
    sol::protected_function fn_wants_input;
    sol::protected_function fn_in_shop;
    sol::protected_function fn_time_up;
    Hooks hooks;
    std::unordered_map<int, Texture> icons;
    std::unordered_map<int, bool> icon_two_frames;
};
