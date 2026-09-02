#include "costume_menu.h"
#include "../../libs/texture.h"
#include "../../libs/input.h"
#include "../../libs/audio.h"
#include "../../libs/filesystem.h"
#include <random>
#include <spdlog/spdlog.h>

CostumeMenu::CostumeMenu(PlayerNum player_num) : player_num(player_num), is_2p(player_num == PlayerNum::P2) {
    auto& info = tex.skin_config[SC::ENTRY_COSTUME_TEXT];
    std::string lang = global_data.config->general.language;
    std::string text_str = info.text[lang];
    float title_x = info.x;
    float title_y = info.y;

    presets_enabled = tex.options[SCO::COSTUME_PRESET_SLOTS];

    if (!load("CostumeMenu", "costume_menu", is_2p, text_str, title_x, title_y)) return;
    fn_update  = lua_object["update"];
    fn_draw_bg = lua_object["draw_bg"];
    fn_draw_fg = lua_object["draw_fg"];
}

CostumeMenu::~CostumeMenu() {
    for (auto& icon : costume_icons)
        ray::UnloadTexture(icon);
}

void CostumeMenu::load_costume_icons(const std::string& subdir, const std::string& json_key) {
    for (auto& icon : costume_icons)
        ray::UnloadTexture(icon);
    costume_icons.clear();
    costume_ids.clear();
    costume_names.clear();
    costume_name_text.reset();
    costume_name_text_index = -1;
    costume_icon_index = 0;

    fs::path dir = resolve_skin_path(fs::path("Models") / subdir);
    if (!fs::exists(dir)) return;

    std::vector<std::pair<int, fs::path>> entries;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".png") {
            try { entries.push_back({std::stoi(e.path().stem().string()), e.path()}); }
            catch (...) {}
        }
    }
    for (auto& [id, path] : entries) {
        costume_ids.push_back(id);
        costume_icons.push_back(ray::LoadTexture(path.string().c_str()));
    }

    fs::path names_path = resolve_skin_path("Models/costume_names.json");
    if (fs::exists(names_path)) {
        try {
            auto doc = read_json_file(names_path);
            if (doc.HasMember(json_key.c_str()) && doc[json_key.c_str()].IsObject()) {
                for (auto& m : doc[json_key.c_str()].GetObject()) {
                    if (!m.value.HasMember("name") || m.value["name"].IsNull()) continue;
                    costume_names[std::stoi(m.name.GetString())] = m.value["name"].GetString();
                }
            }
        } catch (...) {}
    }
}

bool CostumeMenu::is_preset_item(uint32_t item) {
    return item == COSTUME_SELECT::DEFAULT   || item == COSTUME_SELECT::PRESET_1 ||
           item == COSTUME_SELECT::PRESET_2  || item == COSTUME_SELECT::PRESET_3 ||
           item == COSTUME_SELECT::RANDOM_ITEM;
}

void CostumeMenu::load_preset_data() {
    if (preset_data_loaded) return;
    preset_data_loaded = true;

    fs::path dir = tex.resolve_skin_path(fs::path("Models") / "costume_icon");
    if (fs::exists(dir)) {
        for (auto& e : fs::directory_iterator(dir)) {
            if (e.path().extension() != ".png") continue;
            try { preset_pool.push_back(std::stoi(e.path().stem().string())); } catch (...) {}
        }
    }
    std::sort(preset_pool.begin(), preset_pool.end());

    static const std::pair<const char*, uint32_t> KEYS[] = {
        {"default", COSTUME_SELECT::DEFAULT},
        {"cool",    COSTUME_SELECT::PRESET_1},
        {"cute",    COSTUME_SELECT::PRESET_2},
        {"joke",    COSTUME_SELECT::PRESET_3},
        {"random",  COSTUME_SELECT::RANDOM_ITEM},
    };

    fs::path presets_path = tex.resolve_skin_path("Models/costume_presets.json");
    if (fs::exists(presets_path)) {
        try {
            auto doc = read_json_file(presets_path);
            for (auto& [name, item] : KEYS) {
                if (!doc.HasMember(name) || !doc[name].IsArray()) continue;
                std::vector<int> ids;
                for (auto& v : doc[name].GetArray())
                    if (v.IsInt()) ids.push_back(v.GetInt());
                if (!ids.empty()) preset_sets[item] = std::move(ids);
            }
        } catch (...) {}
    }
    if (!preset_sets.count(COSTUME_SELECT::DEFAULT))
        preset_sets[COSTUME_SELECT::DEFAULT] = {0};
}

void CostumeMenu::apply_preset(uint32_t item) {
    load_preset_data();

    const std::vector<int>* set = nullptr;
    auto it = preset_sets.find(item);
    if (it != preset_sets.end() && !it->second.empty()) set = &it->second;
    else if (!preset_pool.empty()) set = &preset_pool;
    if (!set || set->empty()) return;

    bool sticky = (item != COSTUME_SELECT::RANDOM_ITEM);
    if (sticky) {
        auto rolled = preset_rolled.find(item);
        if (rolled != preset_rolled.end()) {
            if (preset_cos_id && *preset_cos_id == rolled->second) return;  // already worn
            preset_cos_id = rolled->second;
            preset_seq++;
            return;
        }
    }

    static std::mt19937 rng{std::random_device{}()};
    int id = (*set)[std::uniform_int_distribution<size_t>(0, set->size() - 1)(rng)];
    if (sticky) preset_rolled[item] = id;
    preset_cos_id = id;
    preset_seq++;
    spdlog::info("costume_preset: player={} item={} pool={} cos={} seq={}",
                 (int)player_num, item, set->size(), id, preset_seq);
}

void CostumeMenu::update(double current_time_ms) {
    call(fn_update, "CostumeMenu:update", current_time_ms);
}

std::optional<int> CostumeMenu::get_index() {
    if (!costume_select_mode) return std::nullopt;
    return costume_icon_index;
}

std::string CostumeMenu::get_costume_name() const {
    if (!costume_select_mode && preset_cos_id) return std::to_string(*preset_cos_id);
    if (costume_ids.empty()) return "";
    return std::to_string(costume_ids[costume_icon_index]);
}

void CostumeMenu::handle_input() {
    if (costume_select_mode) {
        if (!costume_icons.empty()) {
            int n = (int)costume_icons.size();
            if (is_l_kat_pressed(player_num)) {
                costume_icon_index = (costume_icon_index - 1 + n) % n;
                audio.play_sound("kat", VolumePreset::SOUND);
            }
            if (is_r_kat_pressed(player_num)) {
                costume_icon_index = (costume_icon_index + 1) % n;
                audio.play_sound("kat", VolumePreset::SOUND);
            }
            if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
                if (pick_stage == CostumePickStage::HEAD) {
                    picked_head_id = costume_ids[costume_icon_index];
                    pick_stage = CostumePickStage::BODY;
                    load_costume_icons("costume_body_icon", "body");
                } else {
                    confirmed = true;
                }
                audio.play_sound("don", VolumePreset::SOUND);
            }
        }
    } else {
        int prev_index = selected_index;
        if (is_l_kat_pressed(player_num)) {
            selected_index = (selected_index - 1 + NUM_ITEMS) % NUM_ITEMS;
            audio.play_sound("kat", VolumePreset::SOUND);
        }
        if (is_r_kat_pressed(player_num)) {
            selected_index = (selected_index + 1) % NUM_ITEMS;
            audio.play_sound("kat", VolumePreset::SOUND);
        }
        if (presets_enabled && selected_index != prev_index) {
            if (is_preset_item(ITEMS[selected_index])) {
                apply_preset(ITEMS[selected_index]);
            } else if (preset_cos_id) {
                preset_cos_id.reset();
                preset_seq++;
            }
        }

        if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
            if (ITEMS[selected_index] == COSTUME_SELECT::COSTUME) {
                costume_select_mode = true;
                pick_stage = CostumePickStage::NONE;
                load_costume_icons("costume_icon", "costume");
                audio.play_sound("don", VolumePreset::SOUND);
            } else if (ITEMS[selected_index] == COSTUME_SELECT::HEAD_BODY) {
                costume_select_mode = true;
                pick_stage = CostumePickStage::HEAD;
                picked_head_id = -1;
                load_costume_icons("costume_head_icon", "head");
                audio.play_sound("don", VolumePreset::SOUND);
            } else if (presets_enabled && is_preset_item(ITEMS[selected_index])) {
                if (!preset_cos_id) apply_preset(ITEMS[selected_index]);
                if (preset_cos_id) {
                    confirmed = true;
                    audio.play_sound("don", VolumePreset::SOUND);
                }
            }
        }
    }
}

void CostumeMenu::draw(float x, float y) {
    call(fn_draw_bg, "CostumeMenu:draw_bg", x, y, selected_index, costume_select_mode, preset_seq);

    constexpr float ITEM_W = 80.0f;

    if (costume_select_mode && !costume_icons.empty()) {
        auto& ib = tex.textures[COSTUME_SELECT::ITEM_BOX_1P];
        float base_x = ib->x[0] + x;
        float base_y = ib->y[0] + y;
        int n = (int)costume_icons.size();
        for (int i = 0; i < 5; i++) {
            int idx = ((costume_icon_index - 2 + i) % n + n) % n;
            auto& icon = costume_icons[idx];
            float scale = std::min(ITEM_W / icon.width, ITEM_W / icon.height);
            float dw = icon.width * scale, dh = icon.height * scale;
            float ix = tex.draw_offset_x + base_x + i * ITEM_W + (ITEM_W - dw) / 2.0f;
            float iy = tex.draw_offset_y + base_y + (ITEM_W - dh) / 2.0f;
            float offset = (icon.width - ib->width) / 2.0f;
            ray::DrawTexturePro(icon,
                {0, 0, (float)icon.width, (float)icon.height},
                {ix + offset, iy + offset, dw, dh}, {0, 0}, 0, ray::WHITE);
        }
    }

    call(fn_draw_fg, "CostumeMenu:draw_fg", x, y);

    if (costume_select_mode && !costume_icons.empty()) {
        auto it = costume_names.find(costume_ids[costume_icon_index]);
        if (it != costume_names.end()) {
            if (costume_name_text_index != costume_icon_index) {
                costume_name_text_index = costume_icon_index;
                costume_name_text = std::make_unique<OutlinedText>(it->second, 32, ray::WHITE, ray::BLACK, false);
            }
            auto& th = tex.textures[COSTUME_SELECT::TEXT_HIGHLIGHT_1P];
            float banner_cx = tex.draw_offset_x + th->x[0] + x;
            float banner_cy = tex.draw_offset_y + th->y[0] + y;
            float text_x = banner_cx - costume_name_text->width / 2.0f;
            float text_y = banner_cy - costume_name_text->height / 2.0f;
            costume_name_text->draw({.x = text_x, .y = text_y});
        }
    }
}
