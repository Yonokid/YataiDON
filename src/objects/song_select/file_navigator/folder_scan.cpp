#include "../../../libs/file/filesystem.h"
#include "../../../libs/file/scores.h"
#include "navigation_load.h"
#ifdef SUPPORT_FUMEN
#include "../../../libs/optional/gen3.h"
#include "../../../libs/optional/gen4.h"
#endif

FolderSummary scan_folder(const fs::path& path, const std::string& player_1_id, const std::string& player_2_id,
                          const std::stop_token& stop) {
    std::map<int, Crown> crown;
    std::map<int, Crown> crown_p2;
    int tja_count = 0;
#ifdef SUPPORT_FUMEN
    if (const gen4::Library* library = gen4::library_for(path)) {
        int genre = gen4::genre_of_path(path);
        for (const auto& listing : library->order()) {
            if (genre < 0 || listing.genre_no == genre) {
                ++tja_count;
            }
        }
        return {{}, {}, tja_count};
    }
    if (const gen3::Library* library = gen3::library_for(path)) {
        std::string genre = gen3::genre_of_path(path);
        for (const auto& listing : library->songs()) {
            if (genre.empty() || listing.genre == genre) {
                ++tja_count;
            }
        }
        return {{}, {}, tja_count};
    }
#endif
    std::set<int> disqualified_p1;
    std::set<int> disqualified_p2;

    auto update_crown = [&](const fs::path& file_path, const std::string& player_id, std::map<int, Crown>& out_crown,
                            std::set<int>& disqualified) {
        const auto& hashes = scores_manager.get_hashes(file_path);
        for (int diff = 0; diff < 5; diff++) {
            if (hashes[diff].empty()) {
                continue;
            }
            std::string hash = hashes[diff];
            auto score = scores_manager.get_score(hash, diff, player_id);

            if (!score || score->crown == Crown::NONE) {
                out_crown.erase(diff);
                disqualified.insert(diff);
                continue;
            }

            if (disqualified.contains(diff)) {
                continue;
            }

            if (!out_crown.contains(diff)) {
                out_crown[diff] = score->crown;
            } else {
                out_crown[diff] = std::min(out_crown[diff], score->crown);
            }
        }
    };

    // Errors are stepped over rather than thrown: one unreadable entry deep in
    // a tree should not take the folder box down with it.
    std::error_code scan_ec;
    auto scan = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, scan_ec);
    while (scan != fs::end(scan) && !stop.stop_requested()) {
        const fs::directory_entry& entry = *scan;
        if (entry.path().filename() == "song_list.txt") {
            auto entries = read_song_list(entry.path());
            tja_count += static_cast<int>(entries.size());
            for (const auto& e : entries) {
                if (stop.stop_requested()) {
                    break;
                }

                if (auto found = scores_manager.get_path_by_hash(e.hash)) {
                    update_crown(*found, player_1_id, crown, disqualified_p1);
                    update_crown(*found, player_2_id, crown_p2, disqualified_p2);
                }
            }
            scan.increment(scan_ec);
            if (scan_ec) {
                break;
            }
            continue;
        }
        auto ext = entry.path().extension();
        if (ext == ".tja" || ext == ".osu") {
            tja_count++;
            update_crown(entry.path(), player_1_id, crown, disqualified_p1);
            update_crown(entry.path(), player_2_id, crown_p2, disqualified_p2);
        }

        scan.increment(scan_ec);
        if (scan_ec) {
            break;
        }
    }

    return {std::move(crown), std::move(crown_p2), tja_count};
}
