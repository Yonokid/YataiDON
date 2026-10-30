#include "costume_menu.h"

#include <algorithm>
#include <random>
#include <spdlog/spdlog.h>

#include "../../libs/audio/audio.h"
#include "../../libs/file/filesystem.h"
#include "../../libs/gfx/text.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/input/input.h"

CostumeMenu::CostumeMenu(PlayerNum player_num) : player_num(player_num), is_2p(player_num == PlayerNum::P2) {
    PlayerSoundScope audio_owner(player_num);
    const SkinInfo* info = font_manager.get_text_info("entry_costume_text");
    std::string text_str = font_manager.get_text_label("entry_costume_text");
    float title_x = info ? info->x : 0.0f;
    float title_y = info ? info->y : 0.0f;

    t_item_box = is_2p ? tex.get_texture("costume_select/item_box_2p") : tex.get_texture("costume_select/item_box_1p");

    if (!load("CostumeMenu", "costume_menu", is_2p, text_str, title_x, title_y)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw_bg = lua_object["draw_bg"];
    fn_draw_fg = lua_object["draw_fg"];
    fn_get_position = lua_object["position"];
    if (auto pos = call_r<sol::table>(fn_get_position, "CostumeMenu:position")) {
        pos_x = (*pos)[1].get_or(0.0f);
        pos_y = (*pos)[2].get_or(0.0f);
    }
    fn_is_ready = lua_object["is_ready"];
}

CostumeMenu::~CostumeMenu() {
    for (auto& icon : costume_icons) {
        unload_texture(icon);
    }
}

void CostumeMenu::load_costume_icons(const std::string& subdir, const std::string& json_key) {
    icon_prev_index = -1;
    icon_scroll_dir = 0;
    for (auto& icon : costume_icons) {
        unload_texture(icon);
    }
    costume_icons.clear();
    costume_ids.clear();
    costume_names.clear();
    costume_icon_index = 0;

    fs::path dir = tex.resolve_skin_path(fs::path("Models") / subdir);
    if (!fs::exists(dir)) {
        return;
    }

    std::vector<std::pair<int, fs::path>> entries;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".png") {
            try {
                entries.emplace_back(std::stoi(e.path().stem().string()), e.path());
            } catch (const std::exception& ex) {
                spdlog::warn("CostumeMenu: skipping icon with non-numeric filename {}: {}", e.path().string(),
                             ex.what());
            }
        }
    }
    std::ranges::sort(entries, [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& [id, path] : entries) {
        Texture t = load_texture(path.string().c_str());
        if (t.id == 0 || t.width <= 0 || t.height <= 0) {
            spdlog::warn("CostumeMenu: failed to load icon {}", path.string());
            continue;
        }
        costume_ids.push_back(id);
        costume_icons.push_back(t);
    }

    fs::path names_path = tex.resolve_skin_path("Models/costume_names.json");
    if (fs::exists(names_path)) {
        try {
            auto doc = read_json_file(names_path);
            if (doc.HasMember(json_key.c_str()) && doc[json_key.c_str()].IsObject()) {
                for (auto& m : doc[json_key.c_str()].GetObject()) {
                    if (!m.value.HasMember("name") || m.value["name"].IsNull()) {
                        continue;
                    }
                    costume_names[std::stoi(m.name.GetString())] = m.value["name"].GetString();
                }
            }
        } catch (const std::exception& ex) {
            spdlog::warn("CostumeMenu: failed to parse {}: {}", names_path.string(), ex.what());
        }
    }
}

bool CostumeMenu::is_preset_item(const std::string& item) {
    return item == "costume_select/default" || item == "costume_select/preset_1" || item == "costume_select/preset_2" ||
           item == "costume_select/preset_3" || item == "costume_select/random_item";
}

void CostumeMenu::load_preset_data() {
    if (preset_data_loaded) {
        return;
    }
    preset_data_loaded = true;

    fs::path dir = tex.resolve_skin_path(fs::path("Models") / "costume_icon");
    if (fs::exists(dir)) {
        for (const auto& e : fs::directory_iterator(dir)) {
            if (e.path().extension() != ".png") {
                continue;
            }
            try {
                preset_pool.push_back(std::stoi(e.path().stem().string()));
            } catch (const std::exception& ex) {
                spdlog::warn("CostumeMenu: skipping preset icon with non-numeric filename {}: {}", e.path().string(),
                             ex.what());
            }
        }
    }
    std::ranges::sort(preset_pool);

    static const std::pair<const char*, const char*> KEYS[] = {
        {"default", "costume_select/default"},    {"cool", "costume_select/preset_1"},
        {"cute", "costume_select/preset_2"},      {"joke", "costume_select/preset_3"},
        {"random", "costume_select/random_item"},
    };

    fs::path presets_path = tex.resolve_skin_path("Models/costume_presets.json");
    if (fs::exists(presets_path)) {
        try {
            auto doc = read_json_file(presets_path);
            for (const auto& [name, item] : KEYS) {
                if (!doc.HasMember(name) || !doc[name].IsArray()) {
                    continue;
                }
                std::vector<int> ids;
                for (auto& v : doc[name].GetArray()) {
                    if (v.IsInt()) {
                        ids.push_back(v.GetInt());
                    }
                }
                if (!ids.empty()) {
                    preset_sets[item] = std::move(ids);
                }
            }
        } catch (const std::exception& ex) {
            spdlog::warn("CostumeMenu: failed to parse {}: {}", presets_path.string(), ex.what());
        }
    }
    if (!preset_sets.contains("costume_select/default")) {
        preset_sets["costume_select/default"] = {0};
    }
}

void CostumeMenu::apply_preset(const std::string& item) {
    load_preset_data();

    const std::vector<int>* set = nullptr;
    auto it = preset_sets.find(item);
    if (it != preset_sets.end() && !it->second.empty()) {
        set = &it->second;
    } else if (!preset_pool.empty()) {
        set = &preset_pool;
    }
    if (!set || set->empty()) {
        return;
    }

    bool sticky = (item != "costume_select/random_item");
    if (sticky) {
        auto rolled = preset_rolled.find(item);
        if (rolled != preset_rolled.end()) {
            if (preset_cos_id && *preset_cos_id == rolled->second) {
                return; // already worn
            }
            preset_cos_id = rolled->second;
            preset_seq++;
            return;
        }
    }

    static std::mt19937 rng{std::random_device{}()};
    int id = (*set)[std::uniform_int_distribution<size_t>(0, set->size() - 1)(rng)];
    if (sticky) {
        preset_rolled[item] = id;
    }
    preset_cos_id = id;
    preset_seq++;
    spdlog::info("costume_preset: player={} item={} pool={} cos={} seq={}", static_cast<int>(player_num), item,
                 set->size(), id, preset_seq);
}

void CostumeMenu::update(double current_time_ms) {
    PlayerSoundScope audio_owner(player_num);
    now_ms = current_time_ms;
    call(fn_update, "CostumeMenu:update", current_time_ms);
}

std::optional<int> CostumeMenu::get_index() {
    if (!costume_select_mode) {
        return std::nullopt;
    }
    return costume_icon_index;
}

std::string CostumeMenu::get_costume_name() const {
    if (!costume_select_mode && preset_cos_id) {
        return std::to_string(*preset_cos_id);
    }
    if (pick_stage == CostumePickStage::BODY && picked_head_id >= 0 && costume_ids.empty()) {
        return std::to_string(picked_head_id);
    }
    if (costume_ids.empty()) {
        return "";
    }
    return std::to_string(costume_ids[costume_icon_index]);
}

void CostumeMenu::handle_input() {
    PlayerSoundScope audio_owner(player_num);
    if (fn_is_ready.valid() && !call_r<bool>(fn_is_ready, "CostumeMenu:is_ready").value_or(true)) {
        return;
    }
    if (costume_select_mode) {
        if (!costume_icons.empty()) {
            int n = static_cast<int>(costume_icons.size());
            if (menu_left_pressed(player_num)) {
                costume_icon_index = (costume_icon_index - 1 + n) % n;
                audio.play_player_sound("kat", player_num, VolumePreset::SOUND);
            }
            if (menu_right_pressed(player_num)) {
                costume_icon_index = (costume_icon_index + 1) % n;
                audio.play_player_sound("kat", player_num, VolumePreset::SOUND);
            }
            if (menu_confirm_pressed(player_num)) {
                if (pick_stage == CostumePickStage::HEAD) {
                    picked_head_id = costume_ids[costume_icon_index];
                    pick_stage = CostumePickStage::BODY;
                    load_costume_icons("costume_body_icon", "body");
                    if (costume_icons.empty()) {
                        spdlog::warn("CostumeMenu: no body icons found, confirming head only");
                        confirmed = true;
                    }
                } else {
                    confirmed = true;
                }
                audio.play_player_sound("don", player_num, VolumePreset::SOUND);
            }
        }
    } else {
        int prev_index = selected_index;
        if (menu_left_pressed(player_num)) {
            selected_index = (selected_index - 1 + NUM_ITEMS) % NUM_ITEMS;
            audio.play_player_sound("kat", player_num, VolumePreset::SOUND);
        }
        if (menu_right_pressed(player_num)) {
            selected_index = (selected_index + 1) % NUM_ITEMS;
            audio.play_player_sound("kat", player_num, VolumePreset::SOUND);
        }
        if (selected_index != prev_index) {
            if (is_preset_item(ITEMS[selected_index])) {
                apply_preset(ITEMS[selected_index]);
            } else if (preset_cos_id) {
                preset_cos_id.reset();
                preset_seq++;
            }
        }

        if (menu_confirm_pressed(player_num)) {
            if (std::string(ITEMS[selected_index]) == "costume_select/costume") {
                pick_stage = CostumePickStage::NONE;
                load_costume_icons("costume_icon", "costume");
                if (costume_icons.empty()) {
                    spdlog::warn("CostumeMenu: no costume icons found, staying in menu");
                    return;
                }
                costume_select_mode = true;
                audio.play_player_sound("don", player_num, VolumePreset::SOUND);
            } else if (std::string(ITEMS[selected_index]) == "costume_select/head_body") {
                pick_stage = CostumePickStage::HEAD;
                picked_head_id = -1;
                load_costume_icons("costume_head_icon", "head");
                if (costume_icons.empty()) {
                    spdlog::warn("CostumeMenu: no head icons found, staying in menu");
                    return;
                }
                costume_select_mode = true;
                audio.play_player_sound("don", player_num, VolumePreset::SOUND);
            } else if (is_preset_item(ITEMS[selected_index])) {
                if (!preset_cos_id) {
                    apply_preset(ITEMS[selected_index]);
                }
                if (preset_cos_id) {
                    confirmed = true;
                    audio.play_player_sound("don", player_num, VolumePreset::SOUND);
                }
            }
        }
    }
}

void CostumeMenu::draw() {
    PlayerSoundScope audio_owner(player_num);
    const float x = pos_x, y = pos_y;
    call(fn_draw_bg, "CostumeMenu:draw_bg", x, y, selected_index, costume_select_mode, preset_seq,
         static_cast<int>(pick_stage));

    constexpr float ITEM_W = 80.0f;

    if (costume_select_mode && !costume_icons.empty() && t_item_box) {
        float base_x = t_item_box->x[0] + x;
        float base_y = t_item_box->y[0] + y;
        int n = static_cast<int>(costume_icons.size());
        if (costume_icon_index != icon_prev_index) {
            if (icon_prev_index >= 0) {
                int step = ((costume_icon_index - icon_prev_index) % n + n) % n;
                icon_scroll_dir = step == 1 ? 1 : (step == n - 1 ? -1 : 0);
                icon_scroll_start = now_ms;
            }
            icon_prev_index = costume_icon_index;
        }
        float glide =
            icon_scroll_dir * ITEM_W *
            (1.0f - std::clamp(static_cast<float>((now_ms - icon_scroll_start) / ICON_SCROLL_MS), 0.0f, 1.0f));
        base_x += glide;
        int slots = std::min(5, n);
        for (int i = 0; i < slots; i++) {
            int idx = ((costume_icon_index - 2 + i) % n + n) % n;
            auto& icon = costume_icons[idx];
            float scale = std::min(ITEM_W / icon.width, ITEM_W / icon.height);
            float dw = icon.width * scale, dh = icon.height * scale;
            float ix = tex.draw_offset_x + base_x + i * ITEM_W + (ITEM_W - dw) / 2.0f;
            float iy = tex.draw_offset_y + base_y + (ITEM_W - dh) / 2.0f;
            draw_texture_pro(icon, {0, 0, static_cast<float>(icon.width), static_cast<float>(icon.height)},
                             {ix, iy, dw, dh}, {0, 0}, 0, Color::WHITE);
        }
    }

    sol::object name_arg = sol::lua_nil;
    if (costume_select_mode && !costume_icons.empty()) {
        auto it = costume_names.find(costume_ids[costume_icon_index]);
        if (it != costume_names.end()) {
            name_arg = sol::make_object(*script_manager.lua, it->second);
        }
    }
    call(fn_draw_fg, "CostumeMenu:draw_fg", x, y, name_arg);
}
