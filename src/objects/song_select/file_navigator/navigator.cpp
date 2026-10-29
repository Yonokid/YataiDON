#include "navigator.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>

#include "../../../libs/file/filesystem.h"
#include "../../../libs/gfx/text.h"
#include "../../../libs/song_library_loader.h"
#include "../song_select_script.h"
#include "box_back.h"
#include "box_song_osu.h"
#include "color_utils.h"
#include "navigation_scan.h"
#ifdef SUPPORT_FUMEN
#include "../../../libs/optional/gen3.h"
#include "../../../libs/optional/gen4.h"
#endif
#include <chrono>
#include <cmath>
#include <ranges>
#include <type_traits>
#include <utility>

static std::unique_ptr<SongBox> make_song_box(const fs::path& path, const BoxDef& box_def, SongParser parser,
                                              std::array<std::string, 5> hashes) {
    if (path.extension() == ".osu") {
        return std::make_unique<SongBoxOsu>(path, box_def, std::move(parser), std::move(hashes));
    }
    return std::make_unique<SongBox>(path, box_def, std::move(parser), std::move(hashes));
}

// The back box closes a folder, so it carries that folder's genre: the genre
// background behind the cursor (and a skin's board frame) follow it, like the
// cabinet's もどる panel.  The box itself keeps its own colour.
static std::unique_ptr<BackBox> make_back_box(const fs::path& parent_path, const BoxDef* folder = nullptr) {
    BoxDef d;
    d.back_color = BackBox::COLOR;
    d.fore_color = BackBox::COLOR;
    d.texture_index = TextureIndex::NONE;
    d.genre_index = folder ? folder->genre_index : GenreIndex::NAMCO;
    return std::make_unique<BackBox>(parent_path, d);
}

static bool alpha_less(const std::string& a, const std::string& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = std::tolower(static_cast<unsigned char>(a[i]));
        unsigned char cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) {
            return ca < cb;
        }
    }
    return a.size() < b.size();
}

static void apply_song_genre(SongBox* song, const BoxDef& box_def) {
    song->song_genre_index = box_def.genre_index;
    song->song_genre_label = box_def.genre_label;
    if (!box_def.box_color.has_value() && !box_def.back_color.has_value() && !box_def.fore_color.has_value()) {
        return;
    }
    BoxColors colors = resolve_box_colors(box_def.box_color, box_def.back_color, box_def.fore_color);
    song->fore_color = colors.outline;
    song->text_color = colors.text;
}

namespace ch = std::chrono;

// These services must outlive navigation jobs during static destruction.
Navigator::Navigator() : song_library(get_song_library()) {
    init_song_list_writer();
    get_song_library_loader();
}

Navigator::~Navigator() {
    loader.shutdown();
}

void Navigator::emit_wheel_event(int id) {
    wheel_event = id;
    ++wheel_event_seq;
    if ((id == WHEEL_EVENT_OPEN_BEGIN || id == WHEEL_EVENT_OPEN_SWAP) && script) {
        script->start_folder_open();
    }

    if (id != WHEEL_EVENT_OPEN_BEGIN && id != WHEEL_EVENT_OPEN_SWAP && script) {
        script->finish_folder_open();
    }

    if (id == WHEEL_EVENT_OPEN_BEGIN || id == WHEEL_EVENT_CLOSE_BEGIN) {
        wheel_leg_ms = get_current_ms();
    }
}

// The swap waits for the arcade's leg timing only for a skin that draws the legs from the wheel
// events; without one the screen sat empty for the rest of that time.
bool Navigator::swap_is_early(int begin_id) const {
    if (!wheel_events_read || wheel_event != begin_id) {
        return false;
    }
    const double duration_scale =
        global_data.config->general.fast_transitions && begin_id == WHEEL_EVENT_OPEN_BEGIN ? 0.2 : 1.0;

    return (get_current_ms() - wheel_leg_ms) < kSwapDelayMs * duration_scale;
}

void Navigator::load_all_roots() {
    cancel_loading();
    is_inline = false;
    inline_state.reset();
    pending_inline_path.reset();
    pending_inline_folder = nullptr;
    genre_bg.reset();
    genre_bg_end_pos.reset();
    inline_streaming = false;
    items.clear();
    open_index = 0;
    folder_scanner.clear();
    box_def_cache.clear();
    is_processing = true;
    loading_complete = false;
    reloading_roots = true;

    current_path = root_paths.empty() ? fs::path{} : root_paths.back();
    submit_load(make_load_request(LoadRequest::Kind::ROOTS));
}

void Navigator::reset_for_skin_reload() {
    cancel_loading();
    inline_state.reset();
    pending_inline_folder = nullptr;
    pending_inline_path.reset();
    genre_bg.reset();
    items.clear();

    script = nullptr;
    is_inline = false;
    inline_streaming = false;
    wheel_events_read = false;
    is_init = false;
}

void Navigator::preload(std::vector<fs::path> songs_paths) {
    if (is_preloaded) {
        return;
    }

    for (fs::path& root : songs_paths) {
        if (root.filename().empty()) {
            root = root.parent_path();
        }
    }

    root_paths = songs_paths;
    open_index = 0;

    get_song_library_loader().start(songs_paths);

    for (const fs::path& root_path : songs_paths) {
        for (const auto& entry : fs::directory_iterator(root_path)) {
            if (!fs::is_directory(entry) || !folder_scanner.has_def_file(entry.path())) {
                continue;
            }
            BoxDef bd = parse_box_def(entry.path());
            if (bd.collection == "RECENT" && !recent_folder_path) {
                recent_folder_path = entry.path();
            }
            if (bd.collection == "FAVORITE" && !favorite_folder_path) {
                favorite_folder_path = entry.path();
                for (const auto& e : read_song_list(entry.path() / "song_list.txt")) {
                    favorite_songs.insert(e.title + "|" + e.subtitle);
                }
            }
        }
    }
    is_preloaded = true;
}

void Navigator::init(std::vector<fs::path> songs_paths) {
    t_background = tex.get_texture("box/background");
    emit_wheel_event(WHEEL_EVENT_SCENE_ENTRY);
    if (is_init && hide_dan != built_hide_dan) {
        cancel_loading();
        is_inline = false;
        inline_state.reset();
        pending_inline_path.reset();
        pending_inline_folder = nullptr;
        genre_bg.reset();
        genre_bg_end_pos.reset();
        awaiting_diff_sort = false;
        diff_sort_filter.reset();
        reopen_folder_path.reset();
        reopen_song_path.reset();
        items.clear();
        open_index = 0;
        is_init = false;
    }
    built_hide_dan = hide_dan;

    if (!is_init) {
        if (!is_preloaded) {
            preload(std::move(songs_paths));
        }

        load_all_roots();
        is_init = true;
    } else if (!resume_directory_load()) {
        if (global_data.config->limits.song_limit > 0) {
            cancel_loading();
            is_inline = false;
            inline_state.reset();
            pending_inline_path.reset();
            pending_inline_folder = nullptr;
            genre_bg.reset();
            awaiting_diff_sort = false;
            diff_sort_filter.reset();
            load_all_roots();
        } else {
            if (inline_state.has_value()) {
                if (open_index >= 0 && open_index < std::ssize(items) &&
                    dynamic_cast<SongBox*>(items[open_index].get())) {
                    reopen_song_path = items[open_index]->path;
                }

                reopen_folder_path = inline_state->saved_folder_box
                                         ? std::optional<fs::path>(inline_state->saved_folder_box->path)
                                         : std::nullopt;
                if (!reopen_folder_path) {
                    reopen_song_path.reset();
                }
                collapse_inline_now();
            }
            for (auto& item : items) {
                item->reset();
                item->fade_in(0);
            }
            if (pending_inline_folder != nullptr) {
                pending_inline_folder->reset();
                pending_inline_folder->expand_box();
            }
            set_positions(false, 0);
            if (!items.empty()) {
                get_current_item()->expand_box();
            }
        }
    }
    vertical_gallery = tex.options.contains(SCO::HORIZONTAL_SONG_SELECT) && tex.options.at(SCO::HORIZONTAL_SONG_SELECT);
    background_move = dynamic_cast<MoveAnimation*>(tex.get_animation(0));
    background_fade_change = dynamic_cast<FadeAnimation*>(tex.get_animation(5));
    bg_genre_index = items.empty() ? GenreIndex::TUTORIAL : items[open_index]->genre_index;
    last_bg_genre_index = bg_genre_index;
    bg_genre_pending = true;
    if (genre_bg.has_value()) {
        genre_bg->fade_in();
    }
}

void Navigator::collapse_inline_now() {
    if (!inline_state.has_value()) {
        return;
    }

    cancel_loading();

    InlineState& state = *inline_state;
    int end = state.first_song_index + state.songs_count;

    if (state.folder_index < 0 || state.folder_index >= std::ssize(items)) {
        return;
    }

    if (state.saved_folder_box) {
        if (state.first_song_index < 0 || end > std::ssize(items)) {
            return;
        }

        items.erase(items.begin() + state.first_song_index, items.begin() + end);
        items.erase(items.begin() + state.folder_index);
        items.insert(items.begin() + state.folder_index, std::move(state.saved_folder_box));
    }

    open_index = state.folder_index;
    items[open_index]->exit_box();

    inline_state.reset();
    pending_inline_folder = nullptr;
    pending_inline_path.reset();
    genre_bg.reset();
    genre_bg_end_pos.reset();
    is_inline = false;
    is_processing = false;
    inline_streaming = false;
    loading_complete = false;
}

void Navigator::cancel_loading() {
    loader.cancel();
    ++request_id;
    load_results.reset(request_id);
    loading_complete = false;
    summary_loading = false;
    active_summary_paths.clear();

    if (active_directory_load) {
        interrupted_directory_load = std::move(active_directory_load);
        active_directory_load.reset();
    }

    is_processing = false;
    inline_streaming = false;
    reloading_roots = false;
}

bool Navigator::resume_directory_load() {
    if (!interrupted_directory_load) {
        return false;
    }

    auto [kind, path] = std::move(*interrupted_directory_load);
    interrupted_directory_load.reset();

    if (kind == LoadRequest::Kind::ROOTS || global_data.config->limits.song_limit > 0) {
        load_all_roots();
    } else {
        open_index = 0;
        is_processing = true;
        request_directory(path);
    }

    return true;
}

LoadRequest Navigator::make_load_request(LoadRequest::Kind kind, const fs::path& path) {
    LoadRequest request;
    request.id = ++request_id;
    request.kind = kind;
    request.path = path;
    request.roots = root_paths;
    request.index = song_library.snapshot();
    request.box_def = pending_inline_box_def;

    auto has_song_list = [](const fs::path& folder) {
        std::error_code ec;
        return fs::exists(folder / "song_list.txt", ec);
    };

    if (kind == LoadRequest::Kind::DIRECTORY || kind == LoadRequest::Kind::INLINE) {
        request.requires_index =
            has_song_list(path) || (kind == LoadRequest::Kind::INLINE && !request.box_def.collection.empty());
    }

    request.back_def = inline_back_def;
    request.language = global_data.config->general.language;
    request.player_1 = global_data.config->network.access_code_1;
    request.player_2 = global_data.config->network.access_code_2;
    request.score_revision = score_revision;
    request.search = current_search;
    request.hide_dan = hide_dan;
    request.difficulty = diff_sort_filter;
    return request;
}

void Navigator::submit_load(LoadRequest request) {
    if (request.kind == LoadRequest::Kind::ROOTS || request.kind == LoadRequest::Kind::DIRECTORY) {
        active_directory_load = {request.kind, request.path};
    }

    interrupted_directory_load.reset();
    load_results.reset(request_id);
    loading_complete = false;
    summary_loading = request.kind == LoadRequest::Kind::SUMMARIES;
    active_summary_paths = request.summary_paths;
    active_summary_revision = request.score_revision;

    if (synchronous_load) {
        load_results.append(NavigationLoader::execute(request));
    } else {
        loader.submit(std::move(request));
    }
}

void Navigator::request_directory(const fs::path& path) {
    setup_back_box(path, true);

    if (navigation_scan::is_gen3_root(path) && path.filename() == "data" && path.parent_path().filename() == "USRDIR" &&
        !items.empty()) {
        if (auto* back = dynamic_cast<BackBox*>(items.front().get())) {
            back->path = path.parent_path().parent_path().parent_path();
        }
    }

    current_path = path;
    submit_load(make_load_request(LoadRequest::Kind::DIRECTORY, path));
}

void Navigator::request_inline(const fs::path& path, BoxDef box_def) {
    pending_inline_box_def = std::move(box_def);
    submit_load(make_load_request(LoadRequest::Kind::INLINE, path));
}

void Navigator::apply_entry(PreparedEntry prepared, bool inline_mode) {
    std::visit(
        [&](auto&& entry) {
            using Entry = std::remove_cvref_t<decltype(entry)>;

            if constexpr (std::is_same_v<Entry, PreparedFolderSummaryFailure>) {
                if (entry.score_revision == score_revision && failed_folder_summaries.insert(entry.path).second) {
                    spdlog::warn("Folder summary of {} failed: {}", entry.path.string(), entry.error);
                }

                return;
            } else if constexpr (std::is_same_v<Entry, PreparedFolderSummary>) {
                if (entry.score_revision != score_revision) {
                    return;
                }

                folder_summaries[entry.path] = entry.summary;

                auto apply = [&](FolderBox* folder) {
                    if (folder && folder->path == entry.path) {
                        folder->crown = entry.summary.crown;
                        folder->crown_p2 = entry.summary.crown_p2;
                        folder->tja_count = entry.summary.tja_count;
                    }
                };

                for (auto& item : items) {
                    apply(dynamic_cast<FolderBox*>(item.get()));
                }

                apply(pending_inline_folder);
                return;
            } else {
                std::unique_ptr<BaseBox> box;

                if constexpr (std::is_same_v<Entry, PreparedSong>) {
                    auto song =
                        make_song_box(entry.path, entry.box_def, std::move(entry.parser), std::move(entry.hashes));

                    if (entry.song_genre) {
                        apply_song_genre(song.get(), *entry.song_genre);
                    }

                    const auto& titles = song->parser.metadata.title;
                    const auto& subtitles = song->parser.metadata.subtitle;

                    std::string key = localized_text(titles, "en", LocalizedTextFallback::ENGLISH_OR_FIRST) + "|" +
                                      localized_text(subtitles, "en", LocalizedTextFallback::ENGLISH_OR_FIRST);

                    song->is_favorite = entry.is_favorite || favorite_songs.contains(key);
                    box = std::move(song);
                } else if constexpr (std::is_same_v<Entry, PreparedBack>) {
                    box = make_back_box(entry.path, &entry.box_def);
                } else {
                    if (entry.directory_definition) {
                        box_def_cache[entry.path.string()] = std::move(*entry.directory_definition);
                    }

                    if (entry.has_children) {
                        folder_scanner.remember_child_folders(entry.path, *entry.has_children);
                    }

                    auto folder = std::make_unique<FolderBox>(entry.path, entry.box_def);
                    folder->is_osu_folder = entry.is_osu_folder;

                    if (auto cached = folder_summaries.find(entry.path); cached != folder_summaries.end()) {
                        folder->crown = cached->second.crown;
                        folder->crown_p2 = cached->second.crown_p2;
                        folder->tja_count = cached->second.tja_count;
                    }

                    if (entry.tja_count >= 0) {
                        folder->tja_count = entry.tja_count;
                    }

                    box = std::move(folder);
                }

                box->preserve_order = entry.preserve_order;
                box->is_new = entry.is_new;

                if (entry.fade_delay) {
                    box->fade_in(*entry.fade_delay);
                }

                box->preregister_text();

                if (inline_mode && inline_state) {
                    int insert_pos = inline_state->first_song_index + inline_state->songs_count;
                    items.insert(items.begin() + insert_pos, std::move(box));
                    ++inline_state->songs_count;
                } else {
                    items.push_back(std::move(box));
                }
            }
        },
        std::move(prepared));
}

void sort_items(std::vector<std::unique_ptr<BaseBox>>& items, int first_index, int last_index) {
    if (first_index >= 0 && last_index > first_index && last_index < static_cast<int>(items.size())) {

        std::vector<int> back_box_positions;
        std::vector<std::unique_ptr<BaseBox>> sortable;

        for (int i = first_index; i <= last_index; i++) {
            if (items[i]->text_name == "BACK_BOX" || items[i]->preserve_order) {
                back_box_positions.push_back(i - first_index);
            } else {
                sortable.push_back(std::move(items[i]));
            }
        }

        std::ranges::sort(sortable, [](const std::unique_ptr<BaseBox>& a, const std::unique_ptr<BaseBox>& b) {
            return alpha_less(a->text_name, b->text_name);
        });

        int sortable_idx = 0;
        for (int i = first_index; i <= last_index; i++) {
            int relative = i - first_index;
            if (std::ranges::contains(back_box_positions, relative)) {
                // BACK_BOX slot — item was never moved, nothing to do
            } else {
                items[i] = std::move(sortable[sortable_idx++]);
            }
        }
    }
}

void Navigator::invalidate_folder_summaries() {
    if (summary_loading) {
        loader.cancel();
        load_results.reset(++request_id);
        summary_loading = false;
        active_summary_paths.clear();
    }

    ++score_revision;
    folder_summaries.clear();
    failed_folder_summaries.clear();
    summary_player_1 = global_data.config->network.access_code_1;
    summary_player_2 = global_data.config->network.access_code_2;

    for (auto& item : items) {
        if (auto* folder = dynamic_cast<FolderBox*>(item.get())) {
            folder->crown.clear();
            folder->crown_p2.clear();
        }
    }

    if (pending_inline_folder) {
        pending_inline_folder->crown.clear();
        pending_inline_folder->crown_p2.clear();
    }
}

void Navigator::request_folder_summaries() {
    if (is_processing || inline_streaming || pending_inline_path || summary_loading || synchronous_load) {
        return;
    }

    auto request = make_load_request(LoadRequest::Kind::SUMMARIES);

    auto add = [&](FolderBox* folder) {
        if (folder && !folder_summaries.contains(folder->path) && !failed_folder_summaries.contains(folder->path)) {
            request.summary_paths.push_back(folder->path);
        }
    };

    for (auto& item : items) {
        add(dynamic_cast<FolderBox*>(item.get()));
    }

    add(pending_inline_folder);

    if (!request.summary_paths.empty()) {
        submit_load(std::move(request));
    } else {
        --request_id;
    }
}

void Navigator::refresh_scores() {
    invalidate_folder_summaries();
    SongBox* curr_item = dynamic_cast<SongBox*>(get_current_item());
    if (curr_item) {
        curr_item->refresh_scores();
    }
}

void Navigator::flush_pending_boxes() {
    const auto persisted_revision = song_list_write_revision();

    if (persisted_revision != song_list_revision) {
        song_list_revision = persisted_revision;
        invalidate_folder_summaries();
    }

    if (summary_player_1 != global_data.config->network.access_code_1 ||
        summary_player_2 != global_data.config->network.access_code_2) {
        invalidate_folder_summaries();
    }

    load_results.append(loader.take_results());

    if (pending_inline_path) {
        return;
    }

    size_t budget = synchronous_load ? std::numeric_limits<size_t>::max() : 16;

    auto terminal = load_results.apply(budget, score_revision, [&](PreparedEntry entry) {
        try {
            apply_entry(std::move(entry), inline_state.has_value());
        } catch (const std::exception& e) {
            spdlog::warn("Skipping prepared entry: {}", e.what());
        }
    });

    if (terminal) {
        if (terminal->status == LoadResult::Status::FAILED) {
            spdlog::error("Navigation loading failed: {}", terminal->error);
        }

        if (summary_loading) {
            if (terminal->status == LoadResult::Status::FAILED && active_summary_revision == score_revision) {
                for (const auto& path : active_summary_paths) {
                    if (!folder_summaries.contains(path)) {
                        failed_folder_summaries.insert(path);
                    }
                }
            }

            active_summary_paths.clear();
            summary_loading = false;
        } else {
            loading_complete = true;
            reloading_roots = false;
            active_directory_load.reset();
        }
    }

    if (inline_state) {
        genre_bg_start = std::max(0, inline_state->first_song_index - 1);
        genre_bg_end = inline_state->first_song_index + inline_state->songs_count - 1;
    }

    if (loading_complete && inline_state.has_value() && swap_is_early(WHEEL_EVENT_OPEN_BEGIN)) {
        return;
    }

    if (loading_complete) {
        if (open_index >= items.size()) {
            open_index = 0;
        }
        if (inline_state.has_value()) {
            sort_items(items, genre_bg_start, genre_bg_end);
        } else if (items.size() > 1) {
            std::vector<int> sortable_indices;
            std::vector<std::unique_ptr<BaseBox>> sortable;
            int first_sortable = dynamic_cast<BackBox*>(items[0].get()) ? 1 : 0;
            for (int i = first_sortable; i < std::ssize(items); i++) {
                if (!items[i]->preserve_order) {
                    sortable_indices.push_back(i);
                    sortable.push_back(std::move(items[i]));
                }
            }
            std::ranges::sort(sortable, [](const std::unique_ptr<BaseBox>& a, const std::unique_ptr<BaseBox>& b) {
                return alpha_less(a->path.filename().string(), b->path.filename().string());
            });
            for (int j = 0; j < std::ssize(sortable_indices); j++) {
                items[sortable_indices[j]] = std::move(sortable[j]);
            }
        }
        if (restore_cursor_path && !inline_state.has_value()) {
            for (int i = 0; i < std::ssize(items); i++) {
                if (items[i]->path == *restore_cursor_path) {
                    open_index = i;
                    break;
                }
            }
            restore_cursor_path.reset();
        }

        if (inline_state.has_value() && reopen_song_path && reopen_folder_path) {
            const auto& folder = inline_state->saved_folder_box;
            if (folder && folder->path == *reopen_folder_path) {
                int first = inline_state->first_song_index;
                int end = std::min(first + inline_state->songs_count, static_cast<int>(items.size()));
                for (int i = first; i < end; i++) {
                    if (items[i]->path == *reopen_song_path) {
                        open_index = i;
                        break;
                    }
                }
            }
            reopen_folder_path.reset();
            reopen_song_path.reset();
        }

        set_positions(false, 800);
        if (!items.empty()) {
            items[open_index]->expand_box();
        }
        is_processing = false;
        if (inline_streaming && inline_state.has_value()) {
            emit_wheel_event(WHEEL_EVENT_OPEN_SWAP);
        }
        inline_streaming = false;
        loading_complete = false;
    }

    request_folder_summaries();
}

void Navigator::apply_diff_sort(int course, int level, int order) {
    last_diff_sort_result = {course, level};
    diff_sort_filter = std::array<int, 3>{course, level, order};
    awaiting_diff_sort = false;
    begin_inline_load();
}

void Navigator::cancel_diff_sort() {
    awaiting_diff_sort = false;
    diff_sort_filter.reset();
    inline_state.reset();
    pending_inline_path.reset();
    pending_inline_folder = nullptr;
}

void Navigator::toggle_favorite(SongBox* song) {
    if (!favorite_folder_path) {
        return;
    }
    invalidate_folder_summaries(); // song_list.txt counts change
    auto& t = song->parser.metadata.title;
    auto& s = song->parser.metadata.subtitle;
    std::string title = t.contains("en") ? t.at("en") : t.begin()->second;
    std::string subtitle = s.contains("en") ? s.at("en") : s.begin()->second;
    std::string key = title + "|" + subtitle;

    fs::path song_list_path = *favorite_folder_path / "song_list.txt";
    auto entries = read_song_list(song_list_path);

    bool was_favorite = favorite_songs.contains(key);
    if (was_favorite) {
        favorite_songs.erase(key);
        std::erase_if(entries, [&](const SongListEntry& e) { return e.title == title && e.subtitle == subtitle; });
        song->is_favorite = false;
    } else {
        std::string hash = scores_manager.get_single_hash(song->parser.file_path);

        if (hash.empty()) {
            spdlog::warn("Cannot favorite song without a hash: {}", song->path.string());
            return;
        }

        favorite_songs.insert(key);
        entries.insert(entries.begin(), {std::move(hash), title, subtitle});
        song->is_favorite = true;
    }

    queue_song_list_write(song_list_path, entries);
}

void Navigator::add_to_recent(const SongBox* song) {
    if (!recent_folder_path) {
        return;
    }
    invalidate_folder_summaries(); // song_list.txt counts change
    fs::path song_list_path = *recent_folder_path / "song_list.txt";

    auto& titles = song->parser.metadata.title;
    auto& subtitles = song->parser.metadata.subtitle;
    std::string title = titles.contains("en") ? titles.at("en") : titles.begin()->second;
    std::string subtitle = subtitles.contains("en") ? subtitles.at("en") : subtitles.begin()->second;
    std::string hash = scores_manager.get_single_hash(song->parser.file_path);

    if (hash.empty()) {
        spdlog::warn("Cannot record recent song without a hash: {}", song->path.string());
        return;
    }

    auto entries = read_song_list(song_list_path);
    std::erase_if(entries, [&](const SongListEntry& e) { return e.title == title && e.subtitle == subtitle; });
    entries.insert(entries.begin(), {std::move(hash), title, subtitle});
    if (std::ssize(entries) > 25) {
        entries.resize(25);
    }

    queue_song_list_write(song_list_path, entries);
    promote_recent_box(song);
}

void Navigator::promote_recent_box(const SongBox* song) {
    if (!inline_state.has_value() || pending_inline_folder == nullptr) {
        return;
    }
    if (pending_inline_folder->collection != "RECENT") {
        return;
    }

    int first = inline_state->first_song_index;
    int end = first + inline_state->songs_count;
    if (first < 0 || end > std::ssize(items)) {
        return;
    }

    std::vector<int> slots;
    for (int i = first; i < end; i++) {
        if (dynamic_cast<SongBox*>(items[i].get())) {
            slots.push_back(i);
        }
    }

    int pos = -1;
    for (int i = 0; i < std::ssize(slots); i++) {
        if (items[slots[i]]->path == song->path) {
            pos = i;
            break;
        }
    }
    if (pos <= 0) {
        return; // not in this listing, or already first
    }

    auto moved = std::move(items[slots[pos]]);
    for (int i = pos; i > 0; i--) {
        items[slots[i]] = std::move(items[slots[i - 1]]);
    }
    items[slots[0]] = std::move(moved);

    // Keep the cursor on the same song rather than on whatever slid into
    // its slot.
    if (open_index == slots[pos]) {
        open_index = slots[0];
    } else {
        for (int i = 0; i < pos; i++) {
            if (open_index == slots[i]) {
                open_index = slots[i + 1];
                break;
            }
        }
    }
}

BoxDef Navigator::parse_box_def(const fs::path& path) {
    return cached_navigation_box_def(box_def_cache, path, global_data.config->general.language);
}

BoxDef Navigator::parse_box_def_uncached(const fs::path& path) {
    return parse_navigation_box_def(path, global_data.config->general.language);
}

void Navigator::exit_inline() {
    if (!inline_state) {
        return;
    }
    emit_wheel_event(WHEEL_EVENT_CLOSE_BEGIN);
    InlineState& state = *inline_state;

    // Kick off fade_out on all inserted items
    int end = state.first_song_index + state.songs_count;
    for (int i = std::max(0, state.first_song_index - 1); i < end; i++) {
        items[i]->fade_out();
    }

    state.fading_out = true;
    auto& start_box = items[genre_bg_start];
    auto& end_box = items[genre_bg_end];
    float start_pos = start_box->left_bound;
    float end_pos = end_box->right_bound;
    genre_bg->exit(start_pos, end_pos, pending_inline_folder);
    is_inline = false;
    is_processing = true;
}

void Navigator::begin_inline_load() {
    emit_wheel_event(WHEEL_EVENT_OPEN_BEGIN);
    inline_back_def = pending_inline_box_def;

    if (pending_inline_folder) {
        inline_back_def.genre_index = pending_inline_folder->genre_index;
    }

    submit_load(make_load_request(LoadRequest::Kind::INLINE, *pending_inline_path));
    diff_sort_filter.reset();
    bg_genre_pending = false;
    int approx_items =
        (pending_inline_box_def.collection == "RECOMMENDED" || pending_inline_box_def.collection == "DIFFICULTY")
            ? 10
            : pending_inline_folder->tja_count;
    items[open_index]->enter_box();
    genre_bg_start = 0;
    genre_bg_end = 0;
    int approx_total = (approx_items + approx_items / 10) + 1;
    int last_index = open_index + approx_total;

    float offset = last_index - open_index;
    float base_spacing = 100 * tex.screen_scale;
    float side_offset_r = 300 * tex.screen_scale;
    float center_offset = 150 * tex.screen_scale;

    float temp_end_pos = (594 - center_offset) + (offset * base_spacing) + side_offset_r;
    genre_bg_end_pos = (temp_end_pos > tex.screen_width) ? -100.0f : temp_end_pos;

    genre_bg.emplace(items[open_index]->text_name, items[open_index]->back_color, items[open_index]->texture_index,
                     approx_items * 100);
    is_processing = true;
    // The boxes below the opened folder slide out of the way for the inline list: by the room its
    // songs will take (where set_positions puts them once they are in), not always off screen --
    // a folder of two songs pushed its neighbours out of view and back. They ride the genre
    // background's right edge (same delay, duration and linear motion as its move), so no empty
    // gap opens ahead of it. Unknown size (a collection such as NEW counts 0): the background does
    // not widen, so they stay put and set_positions moves them once the songs are in -- sending
    // them off screen left the right side empty for the whole wait.
    if (approx_items > 0) {
        const float edge_delay = genre_bg->stretch->duration * 1.5;
        const float edge_duration = genre_bg->move->duration;
        for (const auto& item : items) {
            if (item->position >
                items[open_index]->position) { // "18" is a '''magic''' number that should fits for almost everything
                item->move_box(
                    std::min(item->position + approx_items * base_spacing, item->position + 18 * base_spacing),
                    edge_duration, edge_delay, std::nullopt);
            }
        }
    }
}

// NOLINTNEXTLINE(performance-unnecessary-value-param): setup_back_box can destroy the item owning the argument.
void Navigator::load_current_directory(const fs::path path) {
    BoxDef box_def = parse_box_def(path);
    bool has_children = folder_scanner.has_child_folders(path);

#ifdef SUPPORT_FUMEN
    bool own_path_is_arcade_root = !gen4::find_data_root(path).empty() || !gen3::find_data_root(path).empty();
#else
    bool own_path_is_arcade_root = false;
#endif
    if (has_children && root_paths.size() > 1 && !reloading_roots && std::ranges::contains(root_paths, path) &&
        !own_path_is_arcade_root) {
        if (inline_state.has_value()) {
            collapse_inline_now();
            return;
        }

        {
            fs::path game;
#ifdef SUPPORT_FUMEN
            game = gen4::find_data_root(current_path);
            if (game.empty()) {
                game = gen3::find_data_root(current_path);
            }
#endif
            if (!game.empty()) {
                restore_cursor_path = game;
            } else if (open_index >= 0 && open_index < std::ssize(items)) {
                restore_cursor_path = items[open_index]->path;
            }
        }
        load_all_roots();
        return;
    }

    if (!has_children && !items.empty()) {
        if (inline_state.has_value()) {
            collapse_inline_now();
            items[open_index]->close_box();
            for (int i = 0; i < std::ssize(items); i++) {
                if (items[i]->path == path) {
                    open_index = i;
                    break;
                }
            }
            set_positions(true, 0);
        }

        InlineState state;
        state.folder_index = open_index;
        state.first_song_index = open_index + 1;
        state.songs_count = 0;
        pending_inline_folder = static_cast<FolderBox*>(items[open_index].get());
        inline_state = std::move(state);
        pending_inline_path = path;
        pending_inline_box_def = box_def;

        if (box_def.collection == "DIFFICULTY") {
            awaiting_diff_sort = true;
            return;
        }

        begin_inline_load();
        return;
    }

    if (is_inline) {
        exit_inline();
        return;
    }

    open_index = 0;
    folder_scanner.clear();
    box_def_cache.clear();
    setup_back_box(path, true);
    cancel_loading();
    is_processing = true;
    request_directory(path);
}

bool Navigator::jump_to_song(const std::string& hash) {
    auto song_path_opt = scores_manager.get_path_by_diff_hash(hash);
    if (!song_path_opt) {
        spdlog::warn("jump_to_song: no song found for hash {}", hash);
        return false;
    }
    return jump_to_song_path(*song_path_opt);
}

bool Navigator::jump_to_song_path(const fs::path& song_path) {
    struct SynchronousLoad {
        bool& flag;
        explicit SynchronousLoad(bool& flag) : flag(flag) { flag = true; }
        ~SynchronousLoad() { flag = false; }
    } synchronous(synchronous_load);

    std::vector<fs::path> chain;
    fs::path cur = song_path.parent_path();
    while (true) {
        fs::path folder = navigation_scan::find_box_def_folder(cur);
        if (folder.empty()) {
            break;
        }
        chain.push_back(folder);
        if (std::ranges::contains(root_paths, folder.parent_path())) {
            break;
        }
        cur = folder.parent_path();
    }
    if (chain.empty()) {
        spdlog::warn("jump_to_song: could not resolve a genre folder for {}", song_path.string());
        return false;
    }
    std::ranges::reverse(chain);

    BoxDef final_box_def = parse_box_def(chain.back());
    if (!final_box_def.collection.empty()) {
        spdlog::warn("jump_to_song: song resolves into a synthetic collection folder, not supported: {}",
                     chain.back().string());
        return false;
    }

    cancel_loading();
    is_inline = false;
    inline_state.reset();
    pending_inline_path.reset();
    pending_inline_folder = nullptr;
    genre_bg.reset();
    awaiting_diff_sort = false;
    diff_sort_filter.reset();

    auto load_full = [&](const fs::path& folder) {
        open_index = 0;
        folder_scanner.clear();
        box_def_cache.clear();
        setup_back_box(folder, true);
        loading_complete = false;
        request_directory(folder);
        flush_pending_boxes();
    };

    load_full(chain.front().parent_path());
    for (size_t i = 0; i + 1 < chain.size(); i++) {
        load_full(chain[i]);
    }

    fs::path final_folder = chain.back();
    bool final_has_children = folder_scanner.has_child_folders(final_folder);

    if (final_has_children || items.empty()) {
        load_full(final_folder);
    } else {
        int folder_index = -1;
        for (int i = 0; i < std::ssize(items); i++) {
            if (items[i]->path == final_folder) {
                folder_index = i;
                break;
            }
        }
        if (folder_index < 0) {
            spdlog::warn("jump_to_song: could not locate folder box for {}", final_folder.string());
            return false;
        }

        items[open_index]->close_box();
        open_index = folder_index;
        set_positions(true, 0);
        items[open_index]->expand_box();
        items[open_index]->enter_box();

        FolderBox* folder_box = static_cast<FolderBox*>(items[open_index].get());
        for (auto& item : items) {
            if (item && item->position > folder_box->position) {
                item->move_box(tex.screen_width + 150, 600);
            }
        }

        InlineState state;
        state.folder_index = open_index;
        state.first_song_index = open_index + 1;
        state.songs_count = 0;
        state.saved_folder_box = std::unique_ptr<FolderBox>(static_cast<FolderBox*>(items[open_index].release()));
        pending_inline_folder = folder_box;
        inline_state = std::move(state);

        setup_back_box(final_folder, false, folder_box);
        genre_bg.emplace(folder_box->text_name, folder_box->back_color, folder_box->texture_index, 1000.0f);

        loading_complete = false;
        is_inline = true;
        request_inline(final_folder, final_box_def);
        flush_pending_boxes();
    }

    if (items.empty()) {
        spdlog::warn("jump_to_song: destination folder loaded no items for {}", song_path.string());
        return false;
    }

    int target_index = -1;
    for (int i = 0; i < std::ssize(items); i++) {
        if (items[i]->path == song_path) {
            target_index = i;
            break;
        }
    }
    if (target_index < 0) {
        spdlog::warn("jump_to_song: song loaded but not found among items: {}", song_path.string());
        return false;
    }

    items[open_index]->close_box();
    open_index = target_index;
    bg_genre_index = items[open_index]->genre_index;
    last_bg_genre_index = bg_genre_index;
    bg_genre_pending = false;
    set_positions(true, 0);
    items[open_index]->expand_box();
    background_fade_change->start();
    is_processing = false;
    return true;
}

void Navigator::setup_back_box(const fs::path& path, bool has_children, const BaseBox* from) {
    // The box that opened this folder is the authority on its genre and colours: a
    // folder without box.def gets them from its name or the gen3/gen4 tables, which
    // parse_box_def(path) alone does not see.
    if (!from) {
        for (auto& b : items) {
            if (b && b->path == path) {
                from = b.get();
                break;
            }
        }
    }
    BoxDef folder = parse_box_def(path);
    if (from) {
        folder.genre_index = from->genre_index;
    }
    // the loaders repeat a back box every ten songs; those wear the same genre
    inline_back_def = folder;
    if (has_children) {
        if (!reloading_roots) {
            items.clear();
        }
        if (std::ranges::contains(root_paths, path)) {
            return;
        }
        items.push_back(make_back_box(path.parent_path(), &folder));
    } else {
        auto back = make_back_box(path.parent_path(), &folder);
        back->fade_in(266);
        items.erase(items.begin() + open_index);
        items.insert(items.begin() + open_index, std::move(back));
    }
}

bool Navigator::is_directory(BaseBox* item) {
    if (item == nullptr) {
        return false;
    }
    if (dynamic_cast<FolderBox*>(item) != nullptr) {
        return true;
    }
    return !is_song(item) && fs::is_directory(item->path);
}

bool Navigator::is_song(BaseBox* item) {
    // What the box is, not what its path looks like.
    return dynamic_cast<SongBox*>(item) != nullptr;
}

BaseBox* Navigator::get_current_item() {
    if (items.empty()) {
        return nullptr;
    }
    return items[open_index].get();
}

void Navigator::set_positions(bool init, float duration) {
    float num_boxes = items.size();
    for (int i = 0; i < num_boxes; i++) {
        float offset = i - open_index;
        if (offset > num_boxes / 2) {
            offset -= num_boxes;
        } else if (offset < -num_boxes / 2) {
            offset += num_boxes;
        }

        float position;
        if (vertical_gallery) {
            float base_spacing = 120 * tex.screen_scale;
            float center_y = 360 * tex.screen_scale;
            float fixed_x = 640 * tex.screen_scale;
            float expand_gap = 90 * tex.screen_scale;
            float horizontal_spacing = 30 * tex.screen_scale;
            if (const SkinInfo* pitch = tex.get_skin_config(SC::SONG_SELECT_ROW_PITCH)) {
                if (pitch->x > 0) {
                    base_spacing = pitch->x;
                }
                if (pitch->y > 0) {
                    expand_gap = pitch->y;
                }
            }
            if (const SkinInfo* curve = tex.get_skin_config(SC::SONG_SELECT_ROW_CURVE); curve && curve->x > 0) {
                horizontal_spacing = curve->x;
            }
            position = center_y + offset * base_spacing;
            if (offset > 0) {
                position += expand_gap;
            } else if (offset < 0) {
                position -= expand_gap;
            }
            items[i]->vertical = true;
            float new_cross = fixed_x + offset * horizontal_spacing;
            if (init || std::abs(position - items[i]->position) >= tex.screen_height) {
                items[i]->set_position(position);
                items[i]->snap_cross(new_cross);
            } else {
                items[i]->glide_cross(new_cross);
                items[i]->move_box(position, duration);
            }
        } else {
            float base_spacing = 100 * tex.screen_scale;
            float center_offset = 150 * tex.screen_scale;
            float side_offset_l = 0 * tex.screen_scale;
            float side_offset_r = 300 * tex.screen_scale;

            position = ((594 * tex.screen_scale) - center_offset) + (offset * base_spacing);
            if (position == (594 * tex.screen_scale) - center_offset) {
                position += center_offset;
            } else if (position > (594 * tex.screen_scale) - center_offset) {
                position += side_offset_r;
            } else {
                position -= side_offset_l;
            }
            items[i]->vertical = false;
            if (init || std::abs(position - items[i]->position) >= tex.screen_width) {
                items[i]->set_position(position);
            } else {
                items[i]->move_box(position, duration);
            }
        }
    }
}

void Navigator::navigate(int delta, bool snap) {
    if (items.empty() || inline_streaming) {
        return;
    }
    emit_wheel_event(WHEEL_EVENT_CURSOR_MOVE);
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    bg_genre_pending = false;
    const auto item_count = static_cast<int>(items.size());
    open_index = ((open_index + delta) % item_count + item_count) % item_count;
    bg_genre_index = items[open_index]->genre_index;
    set_positions(snap, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::move_left() {
    navigate(-1, false);
}
void Navigator::move_right() {
    navigate(+1, false);
}
void Navigator::skip_left() {
    navigate(-10, true);
}
void Navigator::skip_right() {
    navigate(+10, true);
}

void Navigator::enter_diff_select() {
    items[open_index]->enter_box();
    for (int i = 0; i < items.size(); i++) {
        std::unique_ptr<BaseBox>& box = items[i];
        float duration = 800;
        float distance = (150 * tex.screen_scale);
        if (vertical_gallery) {
            float center_y = 360 * tex.screen_scale;
            bool on_screen = box->position > -100 && box->position < tex.screen_height + 100;
            if (on_screen && i != open_index) {
                if (box->position < center_y) {
                    box->move_box(-distance, duration);
                } else {
                    box->move_box(tex.screen_height + distance, duration);
                }
                box->fade_out();
            }
        } else {
            bool on_screen = box->position > -100 && box->position < tex.screen_width + 100;
            if (on_screen && i != open_index) {
                if (box->position < (594 * tex.screen_scale)) {
                    box->move_box(-distance, duration);
                } else {
                    box->move_box(tex.screen_width + distance, duration);
                }
                box->fade_out();
            }
        }
    }
    if (genre_bg.has_value()) {
        genre_bg->fade_out();
    }
}

void Navigator::exit_diff_select() {
    emit_wheel_event(WHEEL_EVENT_COURSE_BACK);
    items[open_index]->exit_box();
    set_positions(false, 500);
    for (auto& box : items) {
        box->fade_in(166);
    }
    if (genre_bg.has_value()) {
        genre_bg->fade_in();
    }
}

void Navigator::update(double current_ms) {
    if (genre_bg.has_value()) {
        if (inline_state && inline_state->fading_out) {
            genre_bg->update(current_ms, pending_inline_folder);
        } else {
            genre_bg->update(current_ms, nullptr);
        }
    }
    background_move->update(current_ms);
    background_fade_change->update(current_ms);

    flush_pending_boxes();

    if (bg_genre_pending && !items.empty()) {
        GenreIndex cursor_genre = items[open_index]->genre_index;
        if (cursor_genre != bg_genre_index) {
            bg_genre_index = cursor_genre;
            last_bg_genre_index = cursor_genre;
        }
        if (loading_complete) {
            bg_genre_pending = false;
        }
    }

    if (pending_inline_path) {
        if (genre_bg.has_value() && genre_bg->is_finished() && !awaiting_diff_sort) {
            inline_state->saved_folder_box =
                std::unique_ptr<FolderBox>(static_cast<FolderBox*>(items[open_index].release()));
            setup_back_box(*pending_inline_path, false);

            is_inline = true;
            inline_state->songs_count = 0; // will be updated as boxes arrive
            pending_inline_path.reset();
            is_processing = false; // flush_pending_boxes will finalise
            inline_streaming = true;
        }
    }

    if (inline_state && inline_state->fading_out) {
        InlineState& state = *inline_state;
        int end = state.first_song_index + state.songs_count;

        bool all_done = true;
        for (int i = state.first_song_index; i < end; i++) {
            if (!items[i]->fade->is_finished || !genre_bg->is_complete()) {
                all_done = false;
                break;
            }
        }

        if (all_done && swap_is_early(WHEEL_EVENT_CLOSE_BEGIN)) {
            all_done = false;
        }

        if (all_done) {
            pending_inline_folder = nullptr;
            items.erase(items.begin() + state.first_song_index, items.begin() + end);
            items.erase(items.begin() + state.folder_index);
            items.insert(items.begin() + state.folder_index, std::move(state.saved_folder_box));
            open_index = state.folder_index;
            inline_state.reset();
            std::vector<float> before;
            before.reserve(items.size());
            for (const auto& box : items) {
                before.push_back(box->position);
            }
            set_positions(false, 500);
            // The boxes the open folder pushed off screen are a screen width or more from their
            // place, so set_positions puts them there at once: slide them in from the screen edge on
            // their side instead (not from where they were: the list wraps, so that can be the far side)
            if (!vertical_gallery) {
                const float center = items[open_index]->position;
                for (size_t i = 0; i < std::min(items.size(), before.size()); ++i) {
                    const auto& box = items[i];
                    const float previous = before[i];
                    const float to = box->position;
                    const bool was_off = previous > tex.screen_width || previous < -100.0f;
                    if (to == previous || !was_off || to < -100.0f || to > tex.screen_width) {
                        continue;
                    }

                    box->set_position(to > center ? tex.screen_width + 100.0f : -200.0f);
                    box->move_box(to, 500);
                }
            }
            items[open_index]->exit_box();
            genre_bg.reset();
            is_processing = false;
            emit_wheel_event(WHEEL_EVENT_CLOSE_SWAP);
        }
    }

    SongBox::service_bgm_resume(current_ms);

    for (auto& box : items) {
        bool on_screen = vertical_gallery ? (box->position > -100 && box->position < tex.screen_height + 100)
                                          : (box->position > -100 && box->position < tex.screen_width + 100);
        if (on_screen && !box->text_loaded) {
            box->load_text();
        }
        box->update(current_ms);
    }
}

float Navigator::get_diff_fade_in() {
    SongBox* current_item = dynamic_cast<SongBox*>(get_current_item());
    if (!current_item || !current_item->diff_fade_in) {
        return 0.0f;
    }
    return current_item->diff_fade_in->attribute;
}

void Navigator::draw_background() {
    if (script && script->draw_background(this)) {
        return;
    }
    if (!t_background) { // a skin without box/background art
        return;
    }

    int width = t_background->width;
    for (int i = 0; i < width * 4; i += width) {
        tex.draw_texture(t_background, {.frame = static_cast<int>(last_bg_genre_index),
                                        .x = static_cast<float>(i - background_move->attribute)});
        tex.draw_texture(t_background, {.frame = static_cast<int>(bg_genre_index),
                                        .x = static_cast<float>(i - background_move->attribute),
                                        .fade = 1.0f - background_fade_change->attribute});
    }
}

void Navigator::draw() {
    if (!vertical_gallery && genre_bg.has_value()) {
        float start_pos;
        float end_pos;

        if ((!items.empty() && (is_processing || !items[open_index]->fade->is_finished)) &&
            pending_inline_folder != nullptr && genre_bg_end_pos.has_value()) {
            start_pos = pending_inline_folder->left_bound;
            end_pos = genre_bg_end_pos.value(); // approximation while loading
        } else if (genre_bg_start < std::ssize(items) && genre_bg_end < std::ssize(items)) {
            start_pos = items[genre_bg_start]->left_bound;
            end_pos = items[genre_bg_end]->right_bound;
        } else {
            genre_bg.reset();
        }

        if (genre_bg.has_value()) {
            FolderBox* folder = pending_inline_folder;
            if (script) {
                script->draw_genre_bg(&genre_bg.value(), start_pos, end_pos, folder);
            }
        }
    }
    for (auto& box : items) {
        bool on_screen = vertical_gallery ? (box->position > -100 && box->position < tex.screen_height + 100)
                                          : (box->position > -100 && box->position < tex.screen_width + 100);
        if (on_screen && script) {
            script->draw_box(box.get());
        }
    }
}

void Navigator::draw_diff_select_bg() {
    if (open_index >= items.size()) {
        return;
    }
    BaseBox* box = items[open_index].get();
    if (script) {
        script->draw_box_bg(box);
    }
}

void Navigator::draw_score_history() {
    if (open_index < items.size() && dynamic_cast<SongBox*>(items[open_index].get()) != nullptr) {
        items[open_index]->draw_score_history();
    }
}

// Course clear counts for the difficulty sort. The levels come from the startup scan (the library's course levels),
// not a parse of every chart on disk: song select waits for this before it opens the difficulty
// sort and before it hands over to the next screen.
Statistics Navigator::get_statistics(const fs::path& path, const std::stop_token& stop) {
    std::atomic<bool> cancelled{stop.stop_requested()};
    std::stop_callback cancellation(stop, [&cancelled] { cancelled = true; });
    Statistics stats;

    for (int course = 0; course <= 4; course++) {
        for (int level = 1; level <= 10; level++) {
            stats[course][level] = CourseStats{};
        }
    }

    if (!song_library.wait(cancelled)) {
        return stats;
    }

    for (const auto& [song_path, levels] : song_library.course_levels_in_subfolders(path, &cancelled)) {
        if (stop.stop_requested()) {
            break;
        }

        const auto& hashes = scores_manager.get_hashes(song_path);
        for (int course = 0; course <= 4; course++) {
            int level = levels[course];
            if (level < 1 || level > 10) {
                continue;
            }

            CourseStats& cs = stats[course][level];
            cs.total++;

            const std::string& hash = hashes[course];
            if (hash.empty()) {
                continue;
            }

            auto score = scores_manager.get_score(hash, course, scores_manager.player_1);
            if (!score.has_value()) {
                continue;
            }

            if (score->crown >= Crown::FC) {
                cs.full_combos++;
            }
            if (score->crown >= Crown::CLEAR) {
                cs.clears++;
            }
        }
    }
    return stats;
}

Navigator navigator;
