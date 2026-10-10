#pragma once

#include <filesystem>
#include <memory>
#include <optional>

namespace song_loudness {
inline constexpr double target_lufs = -14.0;
inline constexpr double peak_ceiling_dbtp = -1.0;
inline constexpr double fallback_gain_db = -3.0;
inline constexpr double skin_gain_db = fallback_gain_db;

struct Measurement {
    double integrated_lufs;
    double true_peak_dbtp;
};

float gain(const Measurement& measurement);
float fallback_gain();
float skin_gain();

class Scanner {
public:
    explicit Scanner(const std::filesystem::path& cache_directory);
    ~Scanner();
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;

    void enqueue(const std::filesystem::path& path, bool priority = false);
    float gain_for(const std::filesystem::path& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
