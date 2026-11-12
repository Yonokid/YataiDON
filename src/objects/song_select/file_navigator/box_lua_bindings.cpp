#include "box_lua_bindings.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "../../../libs/file/script.h"
#include "../../game/exam_caption.h"
#include "../diff_sort.h"
#include "../player.h"
#include "../song_select_script.h"
#include "box_back.h"
#include "box_dan.h"
#include "box_folder.h"
#include "box_song.h"
#include "genre_bg.h"
#include "navigator.h"
#include "text_layout.h"

namespace {

    void draw_folder_image(const Texture& image, sol::table params) {
        float s = tex.screen_scale;
        float scale = params.get_or("scale", 1.0f);
        float x = params.get_or("x", 0.0f);
        float y = params.get_or("y", 0.0f);
        float fade = params.get_or("fade", 1.0f);
        float w = image.width;
        float h = image.height;
        float dw = w * s * scale;
        float dh = h * s * scale;

        float max_w = params.get_or("max_w", 0.0f);
        float max_h = params.get_or("max_h", 0.0f);
        if (dw > 0.0f && dh > 0.0f && (max_w > 0.0f || max_h > 0.0f)) {
            float fw = (max_w > 0.0f) ? max_w / dw : 1.0f;
            float fh = (max_h > 0.0f) ? max_h / dh : 1.0f;
            float f = std::min(fw, fh);
            if (f < 1.0f) {
                dw *= f;
                dh *= f;
            } // shrink only, aspect kept
        }
        sol::optional<float> cx = params["cx"];
        sol::optional<float> cy = params["cy"];
        if (cx) {
            x = *cx - dw / 2.0f;
        }
        if (cy) {
            y = *cy - dh / 2.0f;
        }

        Rect src{0, 0, w, h};
        Rect dest{x, y, dw, dh};
        draw_texture_pro(image, src, dest, Vec2{0, 0}, 0, ::fade(Color::WHITE, fade));
    }
    sol::table modifier_rows_table(ModifierSelector& m) {
        sol::table out = script_manager.lua->create_table();
        auto rows = m.lua_rows();
        for (int i = 0; i < std::ssize(rows); i++) {
            sol::table r = script_manager.lua->create_table();
            r["name"] = rows[i].name;
            r["value"] = rows[i].value;
            r["label"] = rows[i].label;
            r["state"] = rows[i].state;
            r["changed"] = rows[i].changed;
            r["enabled"] = rows[i].enabled;
            r["greyed"] = rows[i].greyed;
            out[i + 1] = r;
        }
        return out;
    }

    sol::table modifier_change_table(const ModifierSelector& m) {
        sol::table t = script_manager.lua->create_table();
        t["dir"] = m.lua_change_dir();
        t["fade"] = m.lua_change_fade();
        t["active"] = m.lua_change_active();
        t["move"] = m.lua_change_move();
        t["seq"] = m.lua_change_seq();
        t["id"] = m.lua_id();
        t["arrow_fade"] = m.lua_arrow_fade();
        t["arrow_move"] = m.lua_arrow_move();
        return t;
    }

    sol::table neiro_change_table(const NeiroSelector& n) {
        sol::table t = script_manager.lua->create_table();
        t["dir"] = n.lua_change_dir();
        t["fade"] = n.lua_change_fade();
        t["active"] = n.lua_change_active();
        t["move"] = n.lua_change_move();
        t["seq"] = n.lua_change_seq();
        t["id"] = n.lua_id();
        t["arrow_fade"] = n.lua_arrow_fade();
        t["arrow_move"] = n.lua_arrow_move();
        return t;
    }

} // namespace

void register_song_select_lua_bindings(sol::state& lua) {
    lua.new_usertype<BaseBox>(
        "BaseBox", "box_x", &BaseBox::box_x, "box_y", &BaseBox::box_y, "left_bound", &BaseBox::left_bound,
        "right_bound", &BaseBox::right_bound,
        // read-only properties: box.fade / box.open_fade / box.open_anim stay the animation
        // objects Lua skins index (box.fade.attribute) now that the box owns them by unique_ptr
        "fade", sol::property([](BaseBox& self) { return self.fade.get(); }), "open_fade",
        sol::property([](BaseBox& self) { return self.open_fade.get(); }), "open_anim",
        sol::property([](BaseBox& self) { return self.open_anim.get(); }), "draw_state", &BaseBox::draw_state,
        "lua_kind", &BaseBox::lua_kind, "text_name", &BaseBox::text_name, "path",
        [](BaseBox& self) { return self.path.string(); }, "is_new", &BaseBox::is_new, "genre_frame",
        [](BaseBox& self) { return genre_to_ref_frame(self.genre_index); }, "fore_color",
        [](BaseBox& self) -> sol::object {
            sol::table t = script_manager.lua->create_table(0, 4);
            const Color& c = self.fore_color;
            t["r"] = c.r;
            t["g"] = c.g;
            t["b"] = c.b;
            t["a"] = c.a;
            return t;
        },
        "name", &BaseBox::horizontal_name, "collection", &BaseBox::collection, "collab", &BaseBox::collab,
        "genre_index", [](BaseBox& self) { return static_cast<int>(self.genre_index); }, "texture_index",
        [](BaseBox& self) { return self.texture_index; }, "has_recolor", &BaseBox::has_recolor, "has_gradient",
        &BaseBox::has_gradient, "has_flat_recolor", &BaseBox::has_flat_recolor, "gradient_texture",
        &BaseBox::gradient_texture, "begin_recolor", &BaseBox::begin_recolor, "end_recolor", &BaseBox::end_recolor);

    lua.new_usertype<SongBox>(
        "SongBox", sol::base_classes, sol::bases<BaseBox>(), "text_subtitle", &SongBox::text_subtitle, "subtitle",
        &SongBox::horizontal_subtitle, "subtitle_large", &SongBox::horizontal_subtitle_large, "bpm_text",
        [](SongBox& self) { return self.bpm_text.get(); }, "is_ura", &SongBox::is_ura, "oni_locked",
        &SongBox::oni_locked, "is_favorite", &SongBox::is_favorite, "diff_fade_in", &SongBox::diff_fade_in, "has_ura",
        &SongBox::has_ura, "ex_data_flag", &SongBox::ex_data_flag, "course_info",
        [](SongBox& self, double diff) {
            auto info = self.course_info(static_cast<int>(diff));
            sol::table t = script_manager.lua->create_table(0, 6);
            t["has_course"] = info.has_course;
            t["level"] = info.level;
            t["is_branching"] = info.is_branching;
            t["is_papamama"] = info.is_papamama;
            t["crown"] = info.crown;
            t["rank"] = info.rank;
            t["has_score"] = (diff >= 0 && diff < std::ssize(self.scores) && self.scores[diff].has_value());
            return t;
        },
        "course_info_p2",
        [](SongBox& self, double diff) {
            auto info = self.course_info(static_cast<int>(diff));
            info.crown = 0; // Crown::NONE
            info.rank = 0;  // Rank::_NONE
            if (diff >= 0 && diff < std::ssize(self.scores_p2) && self.scores_p2[diff].has_value()) {
                info.crown = static_cast<int>(self.scores_p2[diff]->crown);
                info.rank = static_cast<int>(self.scores_p2[diff]->rank);
            }
            sol::table t = script_manager.lua->create_table(0, 6);
            t["has_course"] = info.has_course;
            t["level"] = info.level;
            t["is_branching"] = info.is_branching;
            t["is_papamama"] = info.is_papamama;
            t["crown"] = info.crown;
            t["rank"] = info.rank;
            t["has_score"] = (diff >= 0 && diff < std::ssize(self.scores_p2) && self.scores_p2[diff].has_value());
            return t;
        },
        "has_preimage", [](SongBox& self) { return self.preimage.has_value(); },
        // the song's own image scaled into the rectangle (x, y, width, height)
        "draw_preimage",
        [](SongBox& self, float x, float y, float width, float height, float fade) {
            if (!self.preimage.has_value()) {
                return;
            }
            Rect src{0, 0, static_cast<float>(self.preimage->width), static_cast<float>(self.preimage->height)};
            Rect dest{x, y, width, height};
            draw_texture_pro(self.preimage.value(), src, dest, Vec2{0, 0}, 0, ::fade(Color::WHITE, fade));
        });

    lua.new_usertype<FolderBox>(
        "FolderBox", sol::base_classes, sol::bases<BaseBox>(), "tja_count", sol::readonly(&FolderBox::tja_count),
        "entered", sol::readonly(&FolderBox::entered), "enter_fade",
        [](FolderBox& self) { return self.enter_fade.get(); }, "highest_crown",
        [](FolderBox& self) -> sol::object {
            if (self.crown.empty()) {
                return sol::lua_nil;
            }
            auto it = self.crown.rbegin(); // std::map is sorted -> highest diff
            sol::table t = script_manager.lua->create_table(0, 2);
            t["crown"] = static_cast<int>(it->second);
            t["frame"] = std::min(static_cast<int>(Difficulty::URA), it->first);
            return t;
        },
        "highest_crown_p2",
        [](FolderBox& self) -> sol::object {
            if (self.crown_p2.empty()) {
                return sol::lua_nil;
            }
            auto it = self.crown_p2.rbegin();
            sol::table t = script_manager.lua->create_table(0, 2);
            t["crown"] = static_cast<int>(it->second);
            t["frame"] = std::min(static_cast<int>(Difficulty::URA), it->first);
            return t;
        },
        "explanation",
        [](FolderBox& self) {
            sol::table t = script_manager.lua->create_table(3, 0);
            for (int i = 0; i < 3; i++) {
                t[i + 1] = self.explanation[i];
            }
            return t;
        },
        "kind",
        [](FolderBox& self) -> std::string {
            if (self.genre_index == GenreIndex::DAN) {
                return "dan";
            }
            if (!self.collection.empty()) {
                return "sort";
            }
            return "genre";
        },
        "has_box_texture", [](FolderBox& self) { return self.box_texture.has_value(); }, "draw_box_texture",
        [](FolderBox& self, sol::table params) {
            if (self.box_texture.has_value()) {
                draw_folder_image(*self.box_texture, std::move(params));
            }
        },
        "has_box_icon", [](FolderBox& self) { return self.box_icon.has_value(); }, "draw_box_icon",
        [](FolderBox& self, sol::table params) {
            if (self.box_icon.has_value()) {
                draw_folder_image(*self.box_icon, std::move(params));
            }
        });

    lua.new_usertype<BackBox>("BackBox", sol::base_classes, sol::bases<BaseBox>());

    lua.new_usertype<DanBox>(
        "DanBox", sol::base_classes, sol::bases<BaseBox>(), "dan_title", sol::readonly(&DanBox::dan_title), "dan_color",
        sol::readonly(&DanBox::dan_color), "dan_rank", sol::readonly(&DanBox::dan_rank), "clear_mark",
        sol::readonly(&DanBox::clear_mark), "dan_index", sol::readonly(&DanBox::dan_index), "gaiden",
        sol::readonly(&DanBox::gaiden), "is_back", sol::readonly(&DanBox::is_back), "total_notes",
        sol::readonly(&DanBox::total_notes), "song_masked",
        [](DanBox& self, double i) {
            auto k = static_cast<size_t>(i);
            return k < self.song_masked.size() && self.song_masked[k];
        },
        "song_count", [](DanBox& self) { return static_cast<int>(self.songs.size()); }, "song_genre",
        [](DanBox& self, double i) { return self.songs[static_cast<int>(i)].genre_index; }, "song_difficulty",
        [](DanBox& self, double i) { return self.songs[static_cast<int>(i)].difficulty; }, "song_level",
        [](DanBox& self, double i) { return self.songs[static_cast<int>(i)].level; }, "chip_name",
        [](DanBox& self) { return self.name_text(); }, "hori_name", [](DanBox& self) { return self.hori_name.get(); },
        "song_title_text",
        [](DanBox& self, double i) {
            int idx = static_cast<int>(i);
            return (idx >= 0 && idx < std::ssize(self.song_texts)) ? self.song_texts[idx].first.get() : nullptr;
        },
        "song_subtitle_text",
        [](DanBox& self, double i) {
            int idx = static_cast<int>(i);
            return (idx >= 0 && idx < std::ssize(self.song_texts)) ? self.song_texts[idx].second.get() : nullptr;
        },
        "exam_count", [](DanBox& self) { return static_cast<int>(self.exams.size()); }, "exam_type",
        [](DanBox& self, double i) { return self.exams[static_cast<int>(i)].type; }, "exam_red",
        [](DanBox& self, double i) { return self.exams[static_cast<int>(i)].red; }, "exam_gold",
        [](DanBox& self, double i) { return self.exams[static_cast<int>(i)].gold; }, "exam_range",
        [](DanBox& self, double i) { return self.exams[static_cast<int>(i)].range; }, "exam_gothrough",
        [](DanBox& self, double i) { return self.exams[static_cast<int>(i)].gothrough; }, "exam_per_song",
        [](DanBox& self, double i) { return self.exams[static_cast<int>(i)].per_song(); }, "exam_song_count",
        [](DanBox& self, double i) { return static_cast<int>(self.exams[static_cast<int>(i)].song_red.size()); },
        "exam_song_red",
        [](DanBox& self, double i, double j) { return self.exams[static_cast<int>(i)].song_red[static_cast<int>(j)]; },
        "exam_song_gold",
        [](DanBox& self, double i, double j) {
            const Exam& e = self.exams[static_cast<int>(i)];
            int idx = static_cast<int>(j);
            return idx < std::ssize(e.song_gold) ? e.song_gold[idx] : e.song_red[idx];
        },
        "exam_caption",
        [](DanBox& self, double i) {
            return exam_border_text(tex, self.exams[static_cast<int>(i)], global_data.config->general.language);
        },
        "exam_song_caption",
        [](DanBox& self, double i, double j) {
            return exam_border_text(tex, self.exams[static_cast<int>(i)].for_song(static_cast<int>(j)),
                                    global_data.config->general.language);
        });

    lua.new_usertype<DiffSortSelect>(
        "SortWindow", "session", [](DiffSortSelect& s) { return s.lua_session(); }, "diff",
        [](DiffSortSelect& s) { return s.lua_diff(); }, "star", [](DiffSortSelect& s) { return s.lua_star(); }, "order",
        [](DiffSortSelect& s) { return s.lua_order(); }, "song_num", [](DiffSortSelect& s) { return s.lua_song_num(); },
        "phase", [](DiffSortSelect& s) { return s.lua_phase(); }, "alpha",
        [](DiffSortSelect& s) { return s.lua_alpha(); }, "arrow_offset",
        [](DiffSortSelect& s, double row) { return s.lua_arrow_offset(static_cast<int>(row)); },
        // classic (non one-menu) window
        "one_menu", [](DiffSortSelect& s) { return s.lua_one_menu(); }, "box",
        [](DiffSortSelect& s) { return s.lua_box(); }, "level", [](DiffSortSelect& s) { return s.lua_level(); },
        "in_level", [](DiffSortSelect& s) { return s.lua_in_level(); }, "confirmation",
        [](DiffSortSelect& s) { return s.lua_confirmation(); }, "confirm_index",
        [](DiffSortSelect& s) { return s.lua_confirm_index(); }, "num_boxes",
        [](DiffSortSelect& s) { return s.lua_num_boxes(); }, "prev_diff",
        [](DiffSortSelect& s) { return s.lua_prev_diff(); }, "prev_level",
        [](DiffSortSelect& s) { return s.lua_prev_level(); }, "limit",
        [](DiffSortSelect& s, double box) { return s.lua_limit(static_cast<int>(box)); }, "which",
        [](DiffSortSelect& s) { return s.lua_which(); }, "which_index",
        [](DiffSortSelect& s) { return s.lua_which_index(); }, "stats_player",
        [](DiffSortSelect& s) { return s.lua_stats_player(); },
        // total, clears, full combos of a course at one star level
        "stats",
        [](DiffSortSelect& s, double diff, double level) {
            CourseStats c = s.lua_stats(static_cast<int>(diff), static_cast<int>(level));
            sol::table t = script_manager.lua->create_table();
            t["total"] = c.total;
            t["clears"] = c.clears;
            t["full_combos"] = c.full_combos;
            return t;
        },
        // the same, summed over every level of a course
        "sums",
        [](DiffSortSelect& s, double diff) {
            auto c = s.lua_sums(static_cast<int>(diff));
            sol::table t = script_manager.lua->create_table();
            t["total"] = c[0];
            t["clears"] = c[1];
            t["full_combos"] = c[2];
            return t;
        },
        "anim",
        [](DiffSortSelect& s) {
            auto a = s.lua_anim();
            sol::table t = script_manager.lua->create_table();
            t["bg_resize"] = a[0];
            t["fade_in"] = a[1];
            t["flicker"] = a[2];
            t["bounce"] = a[3];
            t["arrow_fade"] = a[4];
            t["arrow_move"] = a[5];
            return t;
        });

    lua.new_usertype<SongSelectPlayer>(
        "SongSelectPlayer", "selected_difficulty",
        [](SongSelectPlayer& self) { return static_cast<int>(self.selected_difficulty); }, "player_num",
        [](SongSelectPlayer& self) { return static_cast<int>(self.player_num); }, "difficulty_decided",
        [](SongSelectPlayer& self) { return self.difficulty_decided(); },
        // state for the cursor drawn under the wheel
        "selector_state",
        [](SongSelectPlayer& self) {
            sol::table t = script_manager.lua->create_table();
            t["selected"] = static_cast<int>(self.selected_difficulty);
            t["prev"] = static_cast<int>(self.prev_diff);
            t["move_right"] = self.diff_select_move_right;
            t["move1"] = self.diff_selector_move_1->attribute;
            t["move1_done"] = self.diff_selector_move_1->is_finished;
            t["move2"] = self.diff_selector_move_2->attribute;
            t["move2_done"] = self.diff_selector_move_2->is_finished;
            t["panel"] = self.neiro_selector.has_value() || self.modifier_selector.has_value();
            auto* song = dynamic_cast<SongBox*>(navigator.get_current_item());
            t["papamama"] = self.solo && song && self.selected_difficulty >= Difficulty::EASY &&
                            song->course_info(static_cast<int>(self.selected_difficulty)).is_papamama;
            return t;
        },
        // state for the difficulty banner behind the wheel
        "bg_diff_state",
        [](SongSelectPlayer& self) {
            sol::table t = script_manager.lua->create_table();
            t["selected"] = static_cast<int>(self.selected_difficulty);
            t["bounce"] = self.selected_diff_bounce->attribute;
            t["fade_in"] = self.selected_diff_fadein->attribute;
            t["highlight_fade"] = self.selected_diff_highlight_fade->attribute;
            t["highlight_on"] =
                self.selected_diff_highlight_fade->is_reversing || self.selected_diff_highlight_fade->is_finished;
            t["text_resize"] = self.selected_diff_text_resize->attribute;
            t["text_fade_in"] = self.selected_diff_text_fadein->attribute;
            t["oni_open_ms"] = self.oni_open_ms; // when this player opened a hidden oni, -1 = never
            return t;
        },
        "neiro_active", [](SongSelectPlayer& self) { return self.neiro_selector.has_value(); }, "modifier_active",
        [](SongSelectPlayer& self) { return self.modifier_selector.has_value(); }, "modifier_offset",
        [](SongSelectPlayer& self) -> sol::optional<float> {
            if (!self.modifier_selector.has_value()) {
                return sol::nullopt;
            }
            const auto& m = self.modifier_selector.value();
            return option_panel_offset(m.move, m.is_confirmed);
        },
        "modifier_rows",
        [](SongSelectPlayer& self) -> sol::object {
            if (!self.modifier_selector.has_value()) {
                return sol::lua_nil;
            }
            return modifier_rows_table(self.modifier_selector.value());
        },
        "modifier_index",
        [](SongSelectPlayer& self) -> sol::optional<int> {
            if (!self.modifier_selector.has_value()) {
                return sol::nullopt;
            }
            return self.modifier_selector.value().lua_index() + 1;
        },
        "modifier_confirmed",
        [](SongSelectPlayer& self) -> sol::optional<bool> {
            if (!self.modifier_selector.has_value()) {
                return sol::nullopt;
            }
            return self.modifier_selector.value().is_confirmed;
        },
        "modifier_change",
        [](SongSelectPlayer& self) -> sol::object {
            if (!self.modifier_selector.has_value()) {
                return sol::lua_nil;
            }
            return modifier_change_table(self.modifier_selector.value());
        },
        "neiro_offset",
        [](SongSelectPlayer& self) -> sol::optional<float> {
            if (!self.neiro_selector.has_value()) {
                return sol::nullopt;
            }
            const auto& n = self.neiro_selector.value();
            return option_panel_offset(n.move, n.is_confirmed);
        },
        "neiro_names",
        [](SongSelectPlayer& self) -> sol::object {
            if (!self.neiro_selector.has_value()) {
                return sol::lua_nil;
            }
            sol::table out = script_manager.lua->create_table();
            const auto& names = self.neiro_selector.value().lua_names();
            for (int i = 0; i < std::ssize(names); i++) {
                out[i + 1] = names[i];
            }
            return out;
        },
        "neiro_index",
        [](SongSelectPlayer& self) -> sol::optional<int> {
            if (!self.neiro_selector.has_value()) {
                return sol::nullopt;
            }
            return self.neiro_selector.value().lua_index() + 1;
        },
        "neiro_change",
        [](SongSelectPlayer& self) -> sol::object {
            if (!self.neiro_selector.has_value()) {
                return sol::lua_nil;
            }
            return neiro_change_table(self.neiro_selector.value());
        },
        // the selectors themselves, for the default option panel (see ModifierPanel / NeiroPanel)
        "modifier_panel",
        [](SongSelectPlayer& self) -> ModifierSelector* {
            return self.modifier_selector.has_value() ? &self.modifier_selector.value() : nullptr;
        },
        "neiro_panel",
        [](SongSelectPlayer& self) -> NeiroSelector* {
            return self.neiro_selector.has_value() ? &self.neiro_selector.value() : nullptr;
        });

    lua.new_usertype<ModifierSelector>(
        "ModifierPanel", "player_num", [](ModifierSelector& m) { return static_cast<int>(m.lua_player()); }, "offset",
        [](ModifierSelector& m) { return option_panel_offset(m.move, m.is_confirmed); }, "rows",
        [](ModifierSelector& m) { return modifier_rows_table(m); }, "index",
        [](ModifierSelector& m) { return m.lua_index() + 1; }, "confirmed",
        [](ModifierSelector& m) { return m.is_confirmed; }, "change",
        [](ModifierSelector& m) { return modifier_change_table(m); });

    lua.new_usertype<NeiroSelector>(
        "NeiroPanel", "player_num", [](NeiroSelector& n) { return static_cast<int>(n.lua_player()); }, "offset",
        [](NeiroSelector& n) { return option_panel_offset(n.move, n.is_confirmed); }, "names",
        [](NeiroSelector& n) {
            sol::table out = script_manager.lua->create_table();
            const auto& names = n.lua_names();
            for (int i = 0; i < std::ssize(names); i++) {
                out[i + 1] = names[i];
            }
            return out;
        },
        "index", [](NeiroSelector& n) { return n.lua_index() + 1; }, "change",
        [](NeiroSelector& n) { return neiro_change_table(n); });

    lua.new_usertype<Navigator>(
        "Navigator", "background_move", &Navigator::background_move_anim, "background_fade_change",
        &Navigator::background_fade_anim, "bg_genre_frame", &Navigator::bg_genre_frame, "last_bg_genre_frame",
        &Navigator::last_bg_genre_frame, "current_folder", &Navigator::lua_current_folder,
        // reading either marks the skin as one that animates the folder legs (see swap_is_early)
        "wheel_event", sol::property([](Navigator& n) {
            n.wheel_events_read = true;
            return n.wheel_event;
        }),
        "wheel_event_seq", sol::property([](Navigator& n) {
            n.wheel_events_read = true;
            return n.wheel_event_seq;
        }));

    lua.new_usertype<GenreBG>("GenreBG", "texture_frame", &GenreBG::texture_frame, "name", &GenreBG::name_text,
                              "has_recolor", &GenreBG::has_recolor, "has_gradient", &GenreBG::has_gradient,
                              "gradient_texture", &GenreBG::gradient_texture, "begin_recolor", &GenreBG::begin_recolor,
                              "end_recolor", &GenreBG::end_recolor, "stretch",
                              sol::property([](GenreBG& self) { return self.stretch.get(); }), "scale",
                              sol::property([](GenreBG& self) { return self.scale.get(); }), "move",
                              sol::property([](GenreBG& self) { return self.move.get(); }), "fade",
                              sol::property([](GenreBG& self) { return self.fade.get(); }), "move_left",
                              sol::property([](GenreBG& self) { return self.move_left.get(); }), "move_right",
                              sol::property([](GenreBG& self) { return self.move_right.get(); }), "is_finished",
                              &GenreBG::is_finished, "is_complete", &GenreBG::is_complete);

    // Text-measurement helpers the Lua port needs to lay out folder explanations the way
    // the C++ FolderBox does (word_wrap needs measure_text_ex, unavailable in Lua).
    lua["text"]["word_wrap"] = [](const std::string& s, double font_size, float spacing, float max_width) {
        return word_wrap(s, static_cast<int>(font_size), spacing, max_width);
    };
    lua["text"]["language_is_cjk"] = [](const std::string& lang) { return language_is_cjk(lang); };
}
