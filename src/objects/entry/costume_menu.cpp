#include "costume_menu.h"
#include "../../libs/texture.h"
#include "../../libs/input.h"
#include "../../libs/audio.h"

CostumeMenu::CostumeMenu(PlayerNum player_num) : player_num(player_num), is_2p(player_num == PlayerNum::P2) {
    auto& info = tex.skin_config[SC::ENTRY_COSTUME_TEXT];
    std::string lang = global_data.config->general.language;
    std::string text_str = info.text[lang];
    float title_x = info.x;
    float title_y = info.y;

    if (!load("CostumeMenu", "costume_menu", is_2p, text_str, title_x, title_y)) return;
    fn_update  = lua_object["update"];
    fn_draw_bg = lua_object["draw_bg"];
    fn_draw_fg = lua_object["draw_fg"];
}

CostumeMenu::~CostumeMenu() {
    for (auto& icon : costume_icons)
        ray::UnloadTexture(icon);
}

void CostumeMenu::load_costume_icons() {
    if (icons_loaded) return;
    icons_loaded = true;

    fs::path dir = fs::path("Skins") / global_data.config->paths.skin / "Models/costume_icon";
    if (!fs::exists(dir)) return;

    std::vector<std::pair<int, fs::path>> entries;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".png") {
            try { entries.push_back({std::stoi(e.path().stem().string()), e.path()}); }
            catch (...) {}
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& [id, path] : entries) {
        costume_ids.push_back(id);
        costume_icons.push_back(ray::LoadTexture(path.string().c_str()));
    }
}

void CostumeMenu::update(double current_time_ms) {
    call(fn_update, "CostumeMenu:update", current_time_ms);
}

std::optional<int> CostumeMenu::get_index() {
    if (!costume_select_mode) return std::nullopt;
    return costume_icon_index;
}

std::string CostumeMenu::get_costume_name() const {
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
                confirmed = true;
                audio.play_sound("don", VolumePreset::SOUND);
            }
        }
    } else {
        if (is_l_kat_pressed(player_num)) {
            selected_index = (selected_index - 1 + NUM_ITEMS) % NUM_ITEMS;
            audio.play_sound("kat", VolumePreset::SOUND);
        }
        if (is_r_kat_pressed(player_num)) {
            selected_index = (selected_index + 1) % NUM_ITEMS;
            audio.play_sound("kat", VolumePreset::SOUND);
        }

        if ((is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) && ITEMS[selected_index] == COSTUME_SELECT::COSTUME) {
            costume_select_mode = true;
            load_costume_icons();
            audio.play_sound("don", VolumePreset::SOUND);
        }
    }
}

void CostumeMenu::draw(float x, float y) {
    call(fn_draw_bg, "CostumeMenu:draw_bg", x, y, selected_index, costume_select_mode);

    if (costume_select_mode && !costume_icons.empty()) {
        auto& ib    = tex.textures[COSTUME_SELECT::ITEM_BOX_1P];
        float base_x = ib->x[0] + x;
        float base_y = ib->y[0] + y;
        constexpr float ITEM_W = 80.0f;
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
}
