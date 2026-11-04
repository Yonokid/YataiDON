#include "reward_shop.h"

#include <ctime>

#include "../../libs/global_data.h"
#include "../total_result/total_result.h"

namespace {
    // 0 is the plain set; the art runs spring, summer, autumn, winter
    int current_season() {
        const std::time_t now = std::time(nullptr);
        const int month = std::localtime(&now)->tm_mon + 1;
        return month >= 3 && month <= 5 ? 1 : month >= 6 && month <= 8 ? 2 : month >= 9 && month <= 11 ? 3 : 4;
    }
} // namespace

RewardShop::RewardShop(bool from_time, const bool joined[2], Hooks hooks_in) : hooks(std::move(hooks_in)) {
    if (!script_manager.lua) {
        return;
    }
    sol::state& lua = *script_manager.lua;

    // Both seats get the same lineup; only what each owns differs
    const ShopSeat& first = network.shop_seat(network.shop_seat(0).answered ? 0 : 1);
    sol::table goods = lua.create_table();
    for (size_t i = 0; i < first.goods.size(); i++) {
        const ShopGood& good = first.goods[i];
        RewardArt art = reward_art(good.type, good.id);
        if (!art.icon.empty() && fs::exists(art.icon)) {
            Texture t = load_texture(art.icon.string().c_str());
            if (t.id != 0) {
                icons[static_cast<int>(i)] = t;
            }
        }
        goods.add(lua.create_table_with("type", good.type, "id", good.id, "cost", good.cost, "name", art.name, "icon",
                                        static_cast<int>(i)));
    }

    sol::table players = lua.create_table();
    for (int i = 0; i < 2; i++) {
        const ShopSeat& seat = network.shop_seat(i);
        const MedalSeat& medals = network.medal_seat(i);
        sol::table owned = lua.create_table();
        for (size_t g = 0; g < seat.goods.size(); g++) {
            owned[g + 1] = seat.goods[g].owned;
        }
        // The lineup fetch follows the claim, so its balance already holds this credit's medals
        const int now = seat.medals - (from_time ? medals.add : 0);
        players[i + 1] = lua.create_table_with("joined", joined[i], "card", seat.answered, "now", now, "add",
                                               from_time ? medals.add : 0, "max", seat.max, "daily",
                                               from_time && medals.daily, "weekly", from_time && medals.weekly,
                                               "monthly", from_time && medals.monthly, "owned", owned);
    }

    sol::table data =
        lua.create_table_with("from_time", from_time, "season", current_season(), "goods", goods, "players", players);
    data["draw_icon"] = [this](int key, float cx, float cy, float size, float alpha) {
        auto it = icons.find(key);
        if (it == icons.end() || it->second.width <= 0 || it->second.height <= 0) {
            return;
        }
        const Texture& t = it->second;
        const float scale = size / std::max(t.width, t.height);
        const float w = t.width * scale, h = t.height * scale;
        draw_texture_pro(t, {0, 0, static_cast<float>(t.width), static_cast<float>(t.height)},
                         {tex.draw_offset_x + cx - w / 2, tex.draw_offset_y + cy - h / 2, w, h}, {0, 0}, 0,
                         fade(Color::WHITE, alpha));
    };
    data["buy"] = [this](int seat, int type, int id) { hooks.buy(seat - 1, type, id); };
    data["preview"] = [this](int seat, int type, int id) { hooks.preview(seat - 1, type, id); };
    data["draw_chara"] = [this](int seat, float x, float y) { hooks.draw_chara(seat - 1, x, y); };
    if (!load("RewardShop", "reward_shop", data)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_on_buy = lua_object["on_buy"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_wants_input = lua_object["wants_input"];
    fn_in_shop = lua_object["in_shop"];
    fn_time_up = lua_object["time_up"];
}

RewardShop::~RewardShop() {
    for (auto& [key, t] : icons) {
        unload_texture(t);
    }
}

void RewardShop::update(double current_ms) {
    call(fn_update, "RewardShop:update", current_ms);
}
void RewardShop::input(int seat, Action action) {
    call(fn_input, "RewardShop:input", seat + 1, static_cast<int>(action));
}
void RewardShop::on_buy(const ShopBuyResult& result) {
    call(fn_on_buy, "RewardShop:on_buy", result.seat + 1, result.status, result.medals);
}
void RewardShop::draw() {
    call(fn_draw, "RewardShop:draw");
}
bool RewardShop::is_done() {
    return call_r<bool>(fn_is_done, "RewardShop:is_done").value_or(true);
}
bool RewardShop::in_shop() {
    return call_r<bool>(fn_in_shop, "RewardShop:in_shop").value_or(false);
}
void RewardShop::time_up() {
    call(fn_time_up, "RewardShop:time_up");
}
bool RewardShop::wants_input(int seat) {
    return call_r<bool>(fn_wants_input, "RewardShop:wants_input", seat + 1).value_or(false);
}
