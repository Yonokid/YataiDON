#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../libs/file/script.h"
#include "../../libs/global_data.h" // IWYU pragma: keep
#include "../../libs/network/network.h"

enum class CostumePickStage {
    NONE,
    HEAD,
    BODY
};

class CostumeMenu : public LuaScript {
public:
    PlayerNum player_num;
    bool is_2p = false;
    int selected_index = 0;
    bool costume_select_mode = false;
    int costume_icon_index = 0;

    bool confirmed = false;

    CostumeMenu(PlayerNum player_num, bool collabo_bonus = false);
    ~CostumeMenu() override;
    void update(double current_time_ms);
    void handle_input();
    void draw();
    std::optional<int> get_index();
    std::string get_costume_name() const;

    CostumePickStage get_pick_stage() const { return pick_stage; }
    int get_picked_head_id() const { return picked_head_id; }

    std::optional<int> get_preset_cos_id() const { return preset_cos_id; }
    int get_preset_seq() const { return preset_seq; }

private:
    sol::protected_function fn_update;
    sol::protected_function fn_draw_bg;
    sol::protected_function fn_draw_fg;
    sol::protected_function fn_get_position;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    sol::protected_function fn_is_ready;

    std::vector<Texture> costume_icons;
    std::vector<int> costume_ids;
    std::unordered_map<int, std::string> costume_names;

    CostumePickStage pick_stage = CostumePickStage::NONE;
    int picked_head_id = -1;

    TextureObject* t_item_box = nullptr;

    static constexpr double ICON_SCROLL_MS = 67.0;
    static constexpr double COLLABO_BONUS_MS = 6000.0;
    bool collabo_bonus = false;
    double open_ms = -1.0;
    bool intro_voice_pending = true;
    void play_voice(const std::string& name) const;
    double now_ms = 0.0;
    int icon_prev_index = -1;
    int icon_scroll_dir = 0;
    double icon_scroll_start = 0.0;

    void load_costume_icons(const std::string& subdir, const std::string& json_key);

    std::optional<int> preset_cos_id;
    int preset_seq = 0;
    std::vector<int> preset_pool;
    std::unordered_map<std::string, std::vector<int>> preset_sets;
    bool preset_data_loaded = false;
    std::unordered_map<std::string, int> preset_rolled;
    void load_preset_data();
    std::optional<OwnedCosmetics> owned_cosmetics;
    const OwnedCosmetics& owned();
    void apply_preset(const std::string& item);
    static bool is_preset_item(const std::string& item);

    static constexpr int NUM_ITEMS = 7;
    static constexpr std::array<const char*, NUM_ITEMS> ITEMS = {
        "costume_select/costume",  "costume_select/default",  "costume_select/head_body",   "costume_select/preset_1",
        "costume_select/preset_2", "costume_select/preset_3", "costume_select/random_item",
    };
};
