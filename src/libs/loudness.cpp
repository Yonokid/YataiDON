#include "loudness.h"
#include <ebur128.h>
#include <sndfile.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <list>
#include <locale>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef SUPPORT_FUMEN
#include "optional/nus3bank.h"
#include "optional/nub.h"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace song_loudness {

float fallback_gain() {
    static const float multiplier = static_cast<float>(std::pow(10.0, fallback_gain_db / 20.0));
    return multiplier;
}

float skin_gain() {
    // Short skin cues cannot all provide meaningful integrated loudness measurements.
    static const float multiplier = static_cast<float>(std::pow(10.0, skin_gain_db / 20.0));
    return multiplier;
}

static bool valid(const Measurement& measurement) {
    return std::isfinite(measurement.integrated_lufs) && std::isfinite(measurement.true_peak_dbtp)
        && measurement.integrated_lufs >= -70.0 && measurement.integrated_lufs <= 24.0
        && measurement.true_peak_dbtp >= -120.0 && measurement.true_peak_dbtp <= 24.0;
}

float gain(const Measurement& measurement) {
    if (!valid(measurement)) {
        return fallback_gain();
    }

    const double db = std::min(target_lufs - measurement.integrated_lufs,
                               peak_ceiling_dbtp - measurement.true_peak_dbtp);

    return static_cast<float>(std::pow(10.0, db / 20.0));
}

namespace {
struct Fingerprint {
    uintmax_t size;
    std::filesystem::file_time_type::rep modified;
    bool operator==(const Fingerprint&) const = default;
};

Fingerprint fingerprint(const std::filesystem::path& path) {
    return {std::filesystem::file_size(path), std::filesystem::last_write_time(path).time_since_epoch().count()};
}

std::string path_key(const std::filesystem::path& path) {
    const auto utf8 = std::filesystem::absolute(path).lexically_normal().generic_u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::string cache_name(const std::string& key) {
    uint64_t hash = 14695981039346656037ULL;

    for (unsigned char byte : key) {
        hash = (hash ^ byte) * 1099511628211ULL;
    }

    std::ostringstream name;
    name << std::hex << hash << ".loudness";
    return name.str();
}

struct Meter {
    ebur128_state* state;
    std::vector<float> stereo;

    explicit Meter(int rate)
        : state(ebur128_init(2, rate, EBUR128_MODE_I | EBUR128_MODE_TRUE_PEAK)) {}

    ~Meter() {
        if (state) ebur128_destroy(&state);
    }

    bool add(const float* samples, size_t frames, int channels) {
        if (!state || channels <= 0) return false;
        stereo.resize(frames * 2);

        for (size_t frame = 0; frame < frames; ++frame) {
            // Match the mixer's centered mono pan, including its attenuation.
            stereo[frame * 2] = samples[frame * channels] * (channels == 1 ? 0.5f : 1.0f);
            stereo[frame * 2 + 1] = channels == 1 ? stereo[frame * 2] : samples[frame * channels + 1];
            if (!std::isfinite(stereo[frame * 2]) || !std::isfinite(stereo[frame * 2 + 1])) return false;
        }

        return ebur128_add_frames_float(state, stereo.data(), frames) == EBUR128_SUCCESS;
    }

    std::optional<Measurement> finish() {
        if (!state) return std::nullopt;
        double integrated;
        double peak = 0.0;

        if (ebur128_loudness_global(state, &integrated) != EBUR128_SUCCESS || !std::isfinite(integrated)) {
            return std::nullopt;
        }

        for (unsigned int channel = 0; channel < 2; ++channel) {
            double value;
            if (ebur128_true_peak(state, channel, &value) != EBUR128_SUCCESS) return std::nullopt;
            peak = std::max(peak, value);
        }

        if (peak <= 0.0 || !std::isfinite(peak)) return std::nullopt;
        return Measurement{integrated, 20.0 * std::log10(peak)};
    }
};

std::optional<Measurement> measure(const std::filesystem::path& path, const std::atomic<bool>& stopping) {
#ifdef SUPPORT_FUMEN
    if (path.extension() == ".nus3bank" || path.extension() == ".nub") {
        gen4::DecodedAudio decoded;
        const bool ok = path.extension() == ".nub" ? gen3::decode_nub(path, decoded) : gen4::decode_nus3bank(path, decoded);
        if (!ok || decoded.channels <= 0 || decoded.sample_rate <= 0) return std::nullopt;
        Meter meter(decoded.sample_rate);
        const size_t frames = decoded.samples.size() / decoded.channels;

        for (size_t offset = 0; offset < frames; offset += 4096) {
            if (stopping || !meter.add(decoded.samples.data() + offset * decoded.channels,
                                      std::min(size_t{4096}, frames - offset), decoded.channels)) {
                return std::nullopt;
            }
        }

        return meter.finish();
    }
#endif

    SF_INFO info{};
#ifdef _WIN32
    SNDFILE* raw = sf_wchar_open(path.c_str(), SFM_READ, &info);
#else
    SNDFILE* raw = sf_open(path.c_str(), SFM_READ, &info);
#endif
    std::unique_ptr<SNDFILE, decltype(&sf_close)> file(raw, sf_close);

    if (!file) return std::nullopt;

    if (info.channels <= 0 || info.samplerate <= 0 || info.frames <= 0) return std::nullopt;
    Meter meter(info.samplerate);
    std::vector<float> samples(4096 * info.channels);
    sf_count_t total = 0;

    while (!stopping) {
        const auto frames = sf_readf_float(file.get(), samples.data(), 4096);
        if (frames == 0) break;
        if (!meter.add(samples.data(), frames, info.channels)) return std::nullopt;
        total += frames;
    }

    if (stopping || total != info.frames || sf_error(file.get()) != SF_ERR_NO_ERROR) return std::nullopt;
    return meter.finish();
}
}

struct Scanner::Impl {
    struct Entry {
        Fingerprint source;
        std::optional<Measurement> measurement;
        bool complete = false;
        bool active = false;
        std::optional<std::list<std::string>::iterator> queued;
    };

    std::filesystem::path directory;
    const std::string temporary_suffix = "." + std::to_string(std::random_device{}()) + ".tmp";
    std::mutex mutex;
    std::condition_variable wake;
    std::atomic<bool> stopping{false};
    std::list<std::string> jobs;
    std::unordered_map<std::string, Entry> entries;
    std::thread worker;

    explicit Impl(const std::filesystem::path& cache_directory)
        : directory(std::filesystem::absolute(cache_directory)) {
        worker = std::thread([this] { run(); });
    }

    ~Impl() {
        stopping = true;
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }

    std::optional<Measurement> load(const std::string& key, const Fingerprint& source) {
        std::ifstream input(directory / cache_name(key));
        input.imbue(std::locale::classic());
        std::string version, stored_key;
        Fingerprint stored{};
        Measurement measured{};

        if (!(input >> version >> std::quoted(stored_key) >> stored.size >> stored.modified
                    >> measured.integrated_lufs >> measured.true_peak_dbtp)
            || version != "YATAIDON_LOUDNESS_V1" || stored_key != key || stored != source
            || !valid(measured)) {
            return std::nullopt;
        }

        return measured;
    }

    void save(const std::string& key, const Fingerprint& source, const Measurement& measured) {
        std::filesystem::create_directories(directory);
        const auto path = directory / cache_name(key);
        auto temporary = path;
        temporary += temporary_suffix;
        {
            std::ofstream output(temporary, std::ios::trunc);
            output.imbue(std::locale::classic());

            output << "YATAIDON_LOUDNESS_V1\n" << std::quoted(key) << '\n'
                   << source.size << ' ' << source.modified << '\n' << std::setprecision(17)
                   << measured.integrated_lufs << ' ' << measured.true_peak_dbtp << '\n';

            output.close();
            if (!output) throw std::runtime_error("Cannot write loudness cache");
        }

#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            throw std::runtime_error("Cannot replace loudness cache");
        }
#else
        std::filesystem::rename(temporary, path);
#endif
    }

    float request(const std::filesystem::path& path, bool priority) {
        if (path.empty()) return fallback_gain();

        try {
            const auto key = path_key(path);
            const auto source = fingerprint(path);
            std::unique_lock guard(mutex);
            auto [position, inserted] = entries.try_emplace(key, Entry{source, {}, false, false, {}});
            auto& entry = position->second;

            if (inserted || entry.source != source) {
                if (entry.queued) jobs.erase(*entry.queued);
                entry = Entry{source, load(key, source), false, false, {}};
                entry.complete = entry.measurement.has_value();
            }

            if (!entry.complete && !entry.active) {
                if (!entry.queued) {
                    jobs.push_back(key);
                    entry.queued = std::prev(jobs.end());
                }

                if (priority) jobs.splice(jobs.begin(), jobs, *entry.queued);
                wake.notify_one();
            }

            return entry.measurement ? song_loudness::gain(*entry.measurement) : fallback_gain();
        } catch (const std::exception& error) {
            spdlog::debug("Cannot queue loudness analysis for {}: {}", path.string(), error.what());
            return fallback_gain();
        }
    }

    void run() {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#elif defined(__linux__)
        setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10);
#elif defined(__APPLE__)
        pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
#endif
        while (!stopping) {
            std::string key;
            Fingerprint source{};
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [this] { return stopping || !jobs.empty(); });
                if (stopping) return;
                key = jobs.front();
                jobs.pop_front();
                auto& entry = entries.at(key);
                entry.queued.reset();
                entry.active = true;
                source = entry.source;
            }

            std::optional<Measurement> measured;

            try {
                const auto path = std::filesystem::path(std::u8string(key.begin(), key.end()));
                measured = measure(path, stopping);
                if (stopping) return;

                if (fingerprint(path) != source) {
                    request(path, true);
                    continue;
                }

                if (measured) {
                    try {
                        save(key, source, *measured);
                    } catch (const std::exception& error) {
                        spdlog::warn("Cannot save loudness cache for {}: {}", key, error.what());
                    }

                    spdlog::debug("Song loudness {}: {:.1f} LUFS, {:.1f} dBTP", key,
                                  measured->integrated_lufs, measured->true_peak_dbtp);
                } else {
                    spdlog::debug("No loudness measurement for {} (silent, too short, or unsupported audio)", key);
                }
            } catch (const std::exception& error) {
                spdlog::warn("Loudness analysis failed for {}: {}", key, error.what());
            }

            std::lock_guard guard(mutex);
            auto& entry = entries.at(key);

            if (entry.source == source) {
                entry.measurement = measured;
                entry.complete = true;
                entry.active = false;
            }
        }
    }
};

Scanner::Scanner(const std::filesystem::path& cache_directory) : impl(std::make_unique<Impl>(cache_directory)) {}
Scanner::~Scanner() = default;

void Scanner::enqueue(const std::filesystem::path& path, bool priority) {
    impl->request(path, priority);
}

float Scanner::gain_for(const std::filesystem::path& path) {
    return impl->request(path, true);
}
}
