#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <vector>

// Paths come from one completed scan. Consumers can count charts and collections
// without walking the same directories again while a menu is being rendered.
class SongFileSnapshot {
    using Path = std::filesystem::path;
    std::vector<Path> roots;
    std::vector<Path> files;

    static bool within(const Path& path, const Path& root) {
        if (root == ".") {
            return !path.is_absolute() && !path.empty() && *path.begin() != "..";
        }
        auto p = path.begin();
        for (auto r = root.begin(); r != root.end(); ++r, ++p) {
            if (p == path.end() || *p != *r) {
                return false;
            }
        }
        return !root.empty();
    }

public:
    SongFileSnapshot(std::vector<Path> roots, std::vector<Path> files)
        : roots(std::move(roots)), files(std::move(files)) {
        auto normalize = [](Path& path) {
            path = path.lexically_normal();
            if (!path.empty() && path.filename().empty()) {
                path = path.parent_path();
            }
        };
        for (auto& path : this->roots) {
            normalize(path);
        }
        for (auto& path : this->files) {
            normalize(path);
        }
        std::sort(this->files.begin(), this->files.end());
        this->files.erase(std::unique(this->files.begin(), this->files.end()), this->files.end());
    }

    std::optional<std::vector<Path>> under(Path path) const {
        path = path.lexically_normal();
        if (!path.empty() && path.filename().empty()) {
            path = path.parent_path();
        }
        if (std::none_of(roots.begin(), roots.end(), [&](const auto& root) { return within(path, root); })) {
            return std::nullopt;
        }
        std::vector<Path> result;
        if (path == ".") {
            for (const auto& file : files) {
                if (within(file, path)) {
                    result.push_back(file);
                }
            }
            return result;
        }
        // A folder's descendants form one contiguous range in component-wise path order.
        for (auto file = std::ranges::lower_bound(files, path); file != files.end() && within(*file, path); ++file) {
            result.push_back(*file);
        }
        return result;
    }
};

// Chart and song_list.txt paths from the completed scan; nullopt outside its roots.
std::optional<std::vector<std::filesystem::path>> get_scanned_song_entries(const std::filesystem::path& directory);
