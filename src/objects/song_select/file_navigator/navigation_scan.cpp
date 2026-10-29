#include "navigation_scan.h"

#include <cwchar>
#include <vector>

#include "../../../platform/platform_windows.h"
#ifdef SUPPORT_FUMEN
#include "../../../libs/optional/gen3.h"
#include "../../../libs/optional/gen4.h"
#endif

namespace fs = std::filesystem;

namespace navigation_scan {

    fs::path find_box_def_folder(const fs::path& song_path) {
        fs::path current = song_path.parent_path();
        while (!current.empty() && current != current.parent_path()) {
            if (fs::exists(current / "box.def")) {
                return current;
            }

            current = current.parent_path();
        }

        return fs::path{};
    }

    bool is_gen4_root(const fs::path& path) {
#ifdef SUPPORT_FUMEN
        std::error_code ec;
        if (!fs::is_directory(path, ec)) {
            return false;
        }

        return gen4::find_data_root(path) == path && gen4::library_for(path) != nullptr;
#else
        (void)path;
        return false;
#endif
    }

    bool is_gen3_root(const fs::path& path) {
#ifdef SUPPORT_FUMEN
        std::error_code ec;
        if (!fs::is_directory(path, ec)) {
            return false;
        }

        return gen3::find_data_root(path) == path && gen3::library_for(path) != nullptr;
#else
        (void)path;
        return false;
#endif
    }

    fs::path gen3_root_at(const fs::path& path) {
#ifdef SUPPORT_FUMEN
        if (is_gen3_root(path)) {
            return path;
        }

        fs::path nested = path / "USRDIR" / "data";
        if (is_gen3_root(nested)) {
            return nested;
        }

        return {};
#else
        (void)path;
        return {};
#endif
    }

    bool is_osu_song_folder(const fs::path& path) {
        if (!fs::is_directory(path)) {
            return false;
        }

        try {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (fs::is_regular_file(entry.path()) && entry.path().extension() == ".osu") {
                    return true;
                }
            }
        } catch (...) {
        }

        return false;
    }

    bool FolderScanner::has_def_file(const std::filesystem::path& path) {
        std::string key = path.string();
        {
            auto it = def_file_cache.find(key);
            if (it != def_file_cache.end()) {
                return it->second;
            }
        }

        auto remember = [&](bool result) {
            if (!stop.stop_requested()) {
                def_file_cache[key] = result;
            }

            return result;
        };

        std::error_code ec;
        if (fs::exists(path / "box.def", ec)) {
            return remember(true);
        }

        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            if (stop.stop_requested()) {
                return false;
            }

            if (!entry.is_directory(ec)) {
                continue;
            }

            if (is_gen4_root(entry.path()) || !gen3_root_at(entry.path()).empty()) {
                continue;
            }

            if (has_def_file(entry.path())) {
                return remember(true);
            }
        }

        return remember(false);
    }

    bool FolderScanner::has_child_folders(const fs::path& path) {
        const std::string key = path.string();
        {
            if (auto it = child_folders_cache.find(key); it != child_folders_cache.end()) {
                return it->second;
            }
        }

        const bool result = scan_child_folders(path);
        if (!stop.stop_requested()) {
            child_folders_cache[key] = result;
        }

        return result;
    }

    bool FolderScanner::scan_child_folders(const fs::path& path) {
#ifdef SUPPORT_FUMEN
        if (gen4::genre_of_path(path) >= 0) {
            return false;
        }

        if (!gen3::genre_of_path(path).empty()) {
            return false;
        }
#endif
        if (is_gen4_root(path) || is_gen3_root(path)) {
            return true;
        }

#ifdef _WIN32
        // The same checks from one listing per child folder: the portable loop listed each child twice and
        // stat'ed it about six times, which took most of a second for a genre of several hundred songs.
        std::vector<Win32DirEntry> entries;
        if (!win32_list_dir(path, entries)) {
            throw fs::filesystem_error("cannot open directory", path,
                                       std::make_error_code(std::errc::no_such_file_or_directory));
        }

        auto named = [](const Win32DirEntry& e, const wchar_t* name) { return _wcsicmp(e.name.c_str(), name) == 0; };
        for (const Win32DirEntry& entry : entries) {
            if (stop.stop_requested()) {
                return false;
            }

            if (!entry.is_dir) {
                continue;
            }

            const fs::path child = path / entry.name;
            std::vector<Win32DirEntry> inner;
            if (!win32_list_dir(child, inner)) {
                continue;
            }

            bool has_fumen = false, has_usrdir = false;
            for (const Win32DirEntry& e : inner) {
                if (stop.stop_requested()) {
                    return false;
                }

                if (named(e, L"box.def")) {
                    return true; // has_def_file
                }

                if (!e.is_dir) {
                    if (fs::path(e.name).extension() == ".osu") {
                        return true; // is_osu_song_folder
                    }
                } else if (named(e, L"fumen")) {
                    has_fumen = true;
                } else if (named(e, L"USRDIR")) {
                    has_usrdir = true;
                }
            }

            // a data root has a fumen folder (gen3 may sit under USRDIR/data)
            if ((has_fumen || has_usrdir) && (is_gen4_root(child) || !gen3_root_at(child).empty())) {
                return true;
            }

            // has_def_file: a box.def further down, not inside a data root
            for (const Win32DirEntry& e : inner) {
                if (stop.stop_requested()) {
                    return false;
                }

                if (!e.is_dir) {
                    continue;
                }

                const fs::path sub = child / e.name;
                if (is_gen4_root(sub) || !gen3_root_at(sub).empty()) {
                    continue;
                }

                if (has_def_file(sub)) {
                    return true;
                }
            }
        }

        return false;
#else
        for (const auto& entry : fs::directory_iterator(path)) {
            if (stop.stop_requested()) {
                return false;
            }

            if ((fs::is_directory(entry.path()) && has_def_file(entry.path())) || is_osu_song_folder(entry.path()) ||
                is_gen4_root(entry.path()) || !gen3_root_at(entry.path()).empty()) {
                return true;
            }
        }

        return false;
#endif
    }

    void FolderScanner::remember_child_folders(const fs::path& path, bool has_children) {
        child_folders_cache[path.string()] = has_children;
    }

    void FolderScanner::clear() {
        def_file_cache.clear();
        child_folders_cache.clear();
    }
} // namespace navigation_scan
