#include "box_lua_bindings.h"
#include "box_song.h"
#include "box_folder.h"
#include "box_back.h"
#include "navigator.h"
#include "../player.h"
#include "../diff_sort.h"
#include "../../../libs/script.h"

#include <algorithm>

void register_song_select_lua_bindings(sol::state& lua) {
    lua.new_usertype<BaseBox>("BaseBox",
        "box_x",           &BaseBox::box_x,
        "box_y",           &BaseBox::box_y,
        "fade",            &BaseBox::fade,
        "open_fade",       &BaseBox::open_fade,
        "open_anim",       &BaseBox::open_anim,
        "bar_anime_count", &BaseBox::bar_anime_count,
        "draw_state",      &BaseBox::draw_state,
        "lua_kind",        &BaseBox::lua_kind,
        "text_name",       &BaseBox::text_name,
        "path", [](BaseBox& self) { return self.path.string(); },
        "is_new",          &BaseBox::is_new,
        "genre_frame", [](BaseBox& self) { return genre_to_ref_frame(self.genre_index); },
        "fore_color", [](BaseBox& self) -> sol::object {
            if (!self.fore_color.has_value()) return sol::lua_nil;
            sol::table t = script_manager.lua->create_table(0, 4);
            const ray::Color& c = self.fore_color.value();
            t["r"] = c.r; t["g"] = c.g; t["b"] = c.b; t["a"] = c.a;
            return t;
        },
        "name",       &BaseBox::horizontal_name,
        "name_large", &BaseBox::horizontal_name_large,
        "collection", &BaseBox::collection,
        "genre_index", [](BaseBox& self) { return (int)self.genre_index; }
    );

    lua.new_usertype<SongBox>("SongBox",
        sol::base_classes, sol::bases<BaseBox>(),
        "text_subtitle",  &SongBox::text_subtitle,
        "subtitle",       &SongBox::horizontal_subtitle,
        "subtitle_large", &SongBox::horizontal_subtitle_large,
        "bpm_text",     [](SongBox& self) { return self.bpm_text.get(); },
        "is_ura",       &SongBox::is_ura,
        "is_favorite",  &SongBox::is_favorite,
        "diff_fade_in", &SongBox::diff_fade_in,
        "has_ura",      &SongBox::has_ura,
        "ex_data_flag", &SongBox::ex_data_flag,
        "course_info", [](SongBox& self, int diff) {
            auto info = self.course_info(diff);
            sol::table t = script_manager.lua->create_table(0, 5);
            t["has_course"]   = info.has_course;
            t["level"]        = info.level;
            t["is_branching"] = info.is_branching;
            t["crown"]        = info.crown;
            t["rank"]         = info.rank;
            return t;
        }
    );

    lua.new_usertype<FolderBox>("FolderBox",
        sol::base_classes, sol::bases<BaseBox>(),
        "tja_count",      &FolderBox::tja_count,
        "kind", [](FolderBox& self) -> std::string {
            if (self.genre_index == GenreIndex::DAN) return "dan";
            if (!self.collection.empty())            return "sort";
            return "genre";
        },
        "has_box_texture", [](FolderBox& self) { return self.box_texture.has_value(); },
        "draw_box_texture", [](FolderBox& self, sol::table params) {
            if (!self.box_texture.has_value()) return;
            float s      = tex.screen_scale;
            float scale  = params.get_or("scale", 1.0f);
            float x      = params.get_or("x", 0.0f);
            float y      = params.get_or("y", 0.0f);
            float fade   = params.get_or("fade", 1.0f);
            float w      = (float)self.box_texture->width;
            float h      = (float)self.box_texture->height;
            float dw     = w * s * scale;
            float dh     = h * s * scale;

            float max_w = params.get_or("max_w", 0.0f);
            float max_h = params.get_or("max_h", 0.0f);
            if (dw > 0.0f && dh > 0.0f && (max_w > 0.0f || max_h > 0.0f)) {
                float fw = (max_w > 0.0f) ? max_w / dw : 1.0f;
                float fh = (max_h > 0.0f) ? max_h / dh : 1.0f;
                float f  = std::min(fw, fh);
                if (f < 1.0f) { dw *= f; dh *= f; }   // shrink only, aspect kept
            }
            sol::optional<float> cx = params["cx"];
            sol::optional<float> cy = params["cy"];
            if (cx) x = *cx - dw / 2.0f;
            if (cy) y = *cy - dh / 2.0f;

            ray::Rectangle src{0, 0, w, h};
            ray::Rectangle dest{x, y, dw, dh};
            ray::DrawTexturePro(self.box_texture.value(), src, dest, ray::Vector2{0, 0}, 0, ray::Fade(ray::WHITE, fade));
        }
    );

    lua.new_usertype<BackBox>("BackBox", sol::base_classes, sol::bases<BaseBox>());

    lua.new_usertype<DiffSortSelect>("SortWindow",
        "session",      [](DiffSortSelect& s) { return s.lua_session(); },
        "diff",         [](DiffSortSelect& s) { return s.lua_diff(); },
        "star",         [](DiffSortSelect& s) { return s.lua_star(); },
        "order",        [](DiffSortSelect& s) { return s.lua_order(); },
        "diff_count",   [](DiffSortSelect& s) { return s.lua_diff_count(); },
        "order_count",  [](DiffSortSelect& s) { return s.lua_order_count(); },
        "song_num",     [](DiffSortSelect& s) { return s.lua_song_num(); },
        "phase",        [](DiffSortSelect& s) { return s.lua_phase(); },
        "alpha",        [](DiffSortSelect& s) { return s.lua_alpha(); },
        "arrow_offset", [](DiffSortSelect& s, int row) { return s.lua_arrow_offset(row); }
    );

    lua.new_usertype<SongSelectPlayer>("SongSelectPlayer",
        "selected_difficulty", [](SongSelectPlayer& self) { return (int)self.selected_difficulty; },
        "player_num",           [](SongSelectPlayer& self) { return (int)self.player_num; },
        "neiro_active",         [](SongSelectPlayer& self) { return self.neiro_selector.has_value(); },
        "modifier_active",      [](SongSelectPlayer& self) { return self.modifier_selector.has_value(); },
        "modifier_offset",      [](SongSelectPlayer& self) -> sol::optional<float> {
            if (!self.modifier_selector.has_value()) return sol::nullopt;
            const auto& m = self.modifier_selector.value();
            float v = (float)m.move->attribute;
            return m.is_confirmed ? v + tex.skin_config[SC::SONG_SELECT_OFFSET].x : -v;
        },
        "modifier_rows", [](SongSelectPlayer& self) -> sol::object {
            if (!self.modifier_selector.has_value()) return sol::lua_nil;
            sol::table out = script_manager.lua->create_table();
            auto rows = self.modifier_selector.value().lua_rows();
            for (int i = 0; i < (int)rows.size(); i++) {
                sol::table r = script_manager.lua->create_table();
                r["name"]    = rows[i].name;
                r["value"]   = rows[i].value;
                r["label"]   = rows[i].label;
                r["state"]   = rows[i].state;
                r["changed"] = rows[i].changed;
                r["enabled"] = rows[i].enabled;
                r["greyed"]  = rows[i].greyed;
                out[i + 1]   = r;
            }
            return out;
        },
        "modifier_index", [](SongSelectPlayer& self) -> sol::optional<int> {
            if (!self.modifier_selector.has_value()) return sol::nullopt;
            return self.modifier_selector.value().lua_index() + 1;
        },
        "modifier_confirmed", [](SongSelectPlayer& self) -> sol::optional<bool> {
            if (!self.modifier_selector.has_value()) return sol::nullopt;
            return self.modifier_selector.value().is_confirmed;
        },
        "modifier_change", [](SongSelectPlayer& self) -> sol::object {
            if (!self.modifier_selector.has_value()) return sol::lua_nil;
            const auto& m = self.modifier_selector.value();
            sol::table t = script_manager.lua->create_table();
            t["dir"]    = m.lua_change_dir();
            t["fade"]   = m.lua_change_fade();
            t["active"] = m.lua_change_active();
            return t;
        },
        "neiro_offset", [](SongSelectPlayer& self) -> sol::optional<float> {
            if (!self.neiro_selector.has_value()) return sol::nullopt;
            const auto& n = self.neiro_selector.value();
            float v = (float)n.move->attribute;
            return n.is_confirmed ? v + tex.skin_config[SC::SONG_SELECT_OFFSET].x : -v;
        },
        "neiro_names", [](SongSelectPlayer& self) -> sol::object {
            if (!self.neiro_selector.has_value()) return sol::lua_nil;
            sol::table out = script_manager.lua->create_table();
            const auto& names = self.neiro_selector.value().lua_names();
            for (int i = 0; i < (int)names.size(); i++) out[i + 1] = names[i];
            return out;
        },
        "neiro_index", [](SongSelectPlayer& self) -> sol::optional<int> {
            if (!self.neiro_selector.has_value()) return sol::nullopt;
            return self.neiro_selector.value().lua_index() + 1;
        }
    );

    lua.new_usertype<Navigator>("Navigator",
        "background_move",        &Navigator::background_move_anim,
        "background_fade_change", &Navigator::background_fade_anim,
        "bg_genre_frame",         &Navigator::bg_genre_frame,
        "last_bg_genre_frame",    &Navigator::last_bg_genre_frame,
        "current_folder", &Navigator::lua_current_folder,
        "wheel_event",     sol::readonly(&Navigator::wheel_event),
        "wheel_event_seq", sol::readonly(&Navigator::wheel_event_seq)
    );
}
