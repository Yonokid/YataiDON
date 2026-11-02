#pragma once

#include <filesystem>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <utility>

namespace navigation_scan {

    std::filesystem::path find_box_def_folder(const std::filesystem::path& song_path);
    bool is_gen4_root(const std::filesystem::path& path);
    bool is_gen3_root(const std::filesystem::path& path);
    std::filesystem::path gen3_root_at(const std::filesystem::path& path);
    bool is_osu_song_folder(const std::filesystem::path& path);

    class FolderScanner {
        std::stop_token stop;
        std::unordered_map<std::string, bool> def_file_cache;
        std::unordered_map<std::string, bool> child_folders_cache;

        bool scan_child_folders(const std::filesystem::path& path);

    public:
        explicit FolderScanner(std::stop_token stop = {}) : stop(std::move(stop)) {}

        bool has_def_file(const std::filesystem::path& path);
        bool has_child_folders(const std::filesystem::path& path);
        void remember_child_folders(const std::filesystem::path& path, bool has_children);
        void clear();
    };

} // namespace navigation_scan
