#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace online_songs {

    using Manifests = std::vector<std::pair<std::string, std::filesystem::path>>;

    bool is_manifest_url(const std::string& tja_path_entry);

    std::filesystem::path mirror_dir(const std::string& url);

    void sync(const Manifests& manifests, std::atomic<float>& progress);

    std::optional<std::array<std::string, 5>> chart_hashes(const std::filesystem::path& chart);

    bool fetch_song(const std::filesystem::path& chart);

} // namespace online_songs
