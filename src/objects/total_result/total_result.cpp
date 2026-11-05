#include "total_result.h"

#include <ctime>
#include <spdlog/spdlog.h>

#include "../../libs/file/filesystem.h"
#include "../../libs/global_data.h"
#include "../../libs/network/network.h"

RewardArt reward_art(int type, int id) {
    const char* key = type == 2   ? "costume"
                      : type == 3 ? "head"
                      : type == 4 ? "body"
                      : type == 5 ? "puchichara"
                                  : nullptr;
    if (!key) {
        return {};
    }
    RewardArt art;
    art.two_frames = type == 5;
    const std::string id_str = std::to_string(id);
    const std::string icon_dir = type == 2   ? std::string("costume_icon")
                                 : type == 5 ? std::string("acce")
                                             : std::string("costume_") + key + "_icon";
    art.icon = tex.resolve_skin_path(fs::path("Models") / icon_dir / (id_str + ".png"));
    fs::path names_path = tex.resolve_skin_path("Models/costume_names.json");
    if (fs::exists(names_path)) {
        try {
            auto doc = read_json_file(names_path);
            if (doc.HasMember(key) && doc[key].IsObject() && doc[key].HasMember(id_str.c_str())) {
                const auto& entry = doc[key][id_str.c_str()];
                if (entry.HasMember("name") && entry["name"].IsString()) {
                    art.name = entry["name"].GetString();
                }
            }
        } catch (const std::exception& ex) {
            spdlog::warn("RewardArt: failed to parse {}: {}", names_path.string(), ex.what());
        }
    }
    return art;
}

TotalResult::TotalResult() {
    if (!script_manager.lua) {
        return;
    }
    sol::state& lua = *script_manager.lua;
    sol::table players = lua.create_table();
    int next_icon = 0;
    for (int i = 0; i < 2; i++) {
        const DonSeat& seat = network.don_seat(i);
        if (!seat.answered) {
            continue;
        }
        sol::table songs = lua.create_table();
        for (size_t s = 0; s < seat.songs.size(); s++) {
            const std::string title = s < global_data.credit_titles[i].size() ? global_data.credit_titles[i][s] : "";
            songs.add(lua.create_table_with("title", title, "points", seat.songs[s]));
        }
        sol::table titles = lua.create_table();
        for (const DonTitle& title : seat.titles) {
            titles.add(lua.create_table_with("name", title.name, "points", title.points));
        }
        sol::table presents = lua.create_table();
        for (const DonPresent& present : seat.presents) {
            RewardArt art = reward_art(present.type, present.id);
            if (!art.icon.empty() && fs::exists(art.icon)) {
                Texture t = load_texture(art.icon.string().c_str());
                if (t.id != 0) {
                    icons[next_icon] = t;
                }
            }
            presents.add(lua.create_table_with("type", present.type, "id", present.id, "name", art.name, "icon",
                                               next_icon++, "next_aim", present.next_aim));
        }
        players[i + 1] = lua.create_table_with("songs", songs, "titles", titles, "presents", presents, "current",
                                               seat.current, "aim", seat.aim);
    }
    const std::time_t now = std::time(nullptr);
    sol::table data = lua.create_table_with("players", players, "month", std::localtime(&now)->tm_mon + 1);
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
    if (!load("TotalResult", "total_result", data)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_skip = lua_object["skip"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_nameplate = lua_object["nameplate"];
}

TotalResult::~TotalResult() {
    for (auto& [key, t] : icons) {
        unload_texture(t);
    }
}

void TotalResult::update(double current_ms) {
    call(fn_update, "TotalResult:update", current_ms);
}
bool TotalResult::skip() {
    return call_r<bool>(fn_skip, "TotalResult:skip").value_or(false);
}
void TotalResult::draw() {
    call(fn_draw, "TotalResult:draw");
}
bool TotalResult::is_done() {
    return call_r<bool>(fn_is_done, "TotalResult:is_done").value_or(true);
}
float TotalResult::nameplate_fade() {
    return call_r<float>(fn_nameplate, "TotalResult:nameplate").value_or(0.0f);
}
