#include "skin_updater.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "android_ca.h"
#include "sha256.h"

#ifndef __EMSCRIPTEN__

#include <cpr/cpr.h>

#ifdef _WIN32
#include "../platform/platform_windows.h"
#endif

#if defined(__ANDROID__)
#include <curl/curl.h>
#endif

namespace fs = std::filesystem;

namespace {

    std::string strip_dot_git(std::string url) {
        if (url.size() >= 4 && url.ends_with(".git")) {
            url.resize(url.size() - 4);
        }
        return url;
    }

    void trim(std::string& s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
    }

    void init_session(cpr::Session& session) {
#if defined(__ANDROID__)
        session.SetOption(android_ca());
        curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_SSL_EC_CURVES, "X25519:P-256:P-384");
#else
        (void)session;
#endif
    }

    cpr::Response get_url(cpr::Session& session, const std::string& url, int32_t timeout_ms,
                          int32_t connect_timeout_ms) {
        session.SetUrl(cpr::Url{url});
        session.SetTimeout(cpr::Timeout{timeout_ms});
        session.SetConnectTimeout(cpr::ConnectTimeout{connect_timeout_ms});
        return session.Get();
    }

    // Hashes the file in pieces rather than reading all of it into memory. Empty on a read error.
    std::string sha256_file(const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return {};
        }
        crypto::Sha256 ctx;
        std::vector<char> buf(1 << 20);
        while (in) {
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            const std::streamsize got = in.gcount();
            if (got > 0) {
                ctx.update(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<size_t>(got));
            }
        }
        if (in.bad()) {
            return {};
        }
        return crypto::to_hex(ctx.finalize());
    }

    // The hash of each skin file as of its last check, so a pass reads only the files whose size or
    // modification time changed since. Kept in cache/skin_hashes/<skin>.txt, one
    // "<hash> <size> <mtime> <path>" line per file (the path last: it may contain spaces).
    struct HashCacheEntry {
        std::string hash;
        uintmax_t size = 0;
        int64_t mtime = 0;
    };
    using HashCache = std::unordered_map<std::string, HashCacheEntry>;

    fs::path hash_cache_path(const fs::path& skin_dir) {
        return fs::path("cache") / "skin_hashes" / (skin_dir.filename().string() + ".txt");
    }

    HashCache load_hash_cache(const fs::path& skin_dir) {
        HashCache cache;
        std::ifstream in(hash_cache_path(skin_dir));
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream fields(line);
            HashCacheEntry e;
            if (!(fields >> e.hash >> e.size >> e.mtime)) {
                continue;
            }
            std::string rel;
            std::getline(fields >> std::ws, rel);
            if (!rel.empty()) {
                cache[rel] = std::move(e);
            }
        }
        return cache;
    }

    void save_hash_cache(const fs::path& skin_dir, const HashCache& cache) {
        const fs::path file = hash_cache_path(skin_dir);
        const fs::path tmp = fs::path(file).concat(".tmp");
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out) {
                return;
            }
            for (const auto& [rel, e] : cache) {
                out << e.hash << ' ' << e.size << ' ' << e.mtime << ' ' << rel << '\n';
            }
            if (!out) {
                return;
            }
        }
        fs::rename(tmp, file, ec);
        if (ec) {
            fs::remove(tmp, ec);
        }
    }

    // Size and modification time of `path`; false if it cannot be read
    bool file_stamp(const fs::path& path, uintmax_t& size, int64_t& mtime) {
        std::error_code ec;
        size = fs::file_size(path, ec);
        if (ec) {
            return false;
        }
        const auto t = fs::last_write_time(path, ec);
        if (ec) {
            return false;
        }
        mtime = static_cast<int64_t>(t.time_since_epoch().count());
        return true;
    }

    // HTTP 200 alone is not enough: a transfer cut short by the timeout still carries the 200 it
    // started with, and its body is only the part that arrived.
    bool fetched(const cpr::Response& r) {
        return r.status_code == 200 && r.error.code == cpr::ErrorCode::OK;
    }

    // Sessions kept for a whole pass: curl keeps each one's connection open, so the skins and files
    // of a server reuse it instead of making a new TLS handshake per skin and per download thread.
    class SessionPool {
    public:
        std::unique_ptr<cpr::Session> take() {
            {
                std::scoped_lock lock(mutex_);
                if (!free_.empty()) {
                    std::unique_ptr<cpr::Session> s = std::move(free_.back());
                    free_.pop_back();
                    return s;
                }
            }
            auto s = std::make_unique<cpr::Session>();
            init_session(*s);
            return s;
        }

        void give_back(std::unique_ptr<cpr::Session> s) {
            std::scoped_lock lock(mutex_);
            free_.push_back(std::move(s));
        }

        // at the end of a pass: closes the connections
        void clear() {
            std::scoped_lock lock(mutex_);
            free_.clear();
        }

    private:
        std::mutex mutex_;
        std::vector<std::unique_ptr<cpr::Session>> free_;
    };

    SessionPool session_pool;

    // a session from the pool, returned to it when this goes out of scope
    struct PooledSession {
        std::unique_ptr<cpr::Session> session = session_pool.take();
        PooledSession() = default;
        PooledSession(const PooledSession&) = delete;
        PooledSession& operator=(const PooledSession&) = delete;
        ~PooledSession() { session_pool.give_back(std::move(session)); }
        cpr::Session& operator*() const { return *session; }
    };

    // Downloads are written to a file in <skin>/.skin-update-tmp and renamed over the target only
    // once complete, so a pass cut short (the game killed, a full disk) leaves the old file, never a
    // half-written one. The folder is emptied at the start and end of each skin's pass.
    constexpr const char* kTempDirName = ".skin-update-tmp";

    fs::path temp_dir(const fs::path& skin_dir) {
        return skin_dir / kTempDirName;
    }

    // Writes `body` to `tmp`, then renames it over `target` (std::filesystem::rename replaces an
    // existing file, on Windows too). On failure the temp file is removed and `target` is untouched.
    bool write_atomically(const fs::path& tmp, const fs::path& target, const std::string& body) {
        std::error_code ec;
        fs::create_directories(tmp.parent_path(), ec);
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                return false;
            }
            out << body;
            out.close();
            if (!out) {
                fs::remove(tmp, ec);
                return false;
            }
        }
        fs::create_directories(target.parent_path(), ec);
        fs::rename(tmp, target, ec);
        if (ec) {
            std::error_code rm_ec;
            fs::remove(tmp, rm_ec);
            return false;
        }
        return true;
    }

} // namespace

void SkinUpdater::set_current_skin(const std::string& name) {
    std::scoped_lock lock(status_mutex_);
    current_skin_ = name;
    current_file_.clear();
    files_done_ = 0;
    files_total_ = 0;
}

void SkinUpdater::set_current_file(const std::string& file) {
    std::scoped_lock lock(status_mutex_);
    current_file_ = file;
}

void SkinUpdater::note_file_updated() {
    std::scoped_lock lock(status_mutex_);
    ++files_updated_;
}

void SkinUpdater::set_progress(int done, int total) {
    std::scoped_lock lock(status_mutex_);
    files_done_ = done;
    files_total_ = total;
}

void SkinUpdater::note_file_done() {
    std::scoped_lock lock(status_mutex_);
    ++files_done_;
}

bool SkinUpdater::update_one_skin(const std::filesystem::path& skin_dir, const std::string& repo_url,
                                  const std::string& branch) {
    set_current_skin(skin_dir.filename().string());
    std::error_code tmp_ec;
    fs::remove_all(temp_dir(skin_dir), tmp_ec); // left by a pass that was cut short

    PooledSession session;

    auto raw_url = [&](const std::string& rel_path) { return repo_url + "/raw/branch/" + branch + "/" + rel_path; };

    auto media_url = [&](const std::string& rel_path) { return repo_url + "/media/branch/" + branch + "/" + rel_path; };

    // a whole manifest or nothing: files it doesn't list get removed below, so a truncated
    // one would delete everything past the cut (and the last line must be complete)
    cpr::Response checksums = get_url(*session, raw_url("checksums.sha256"), 60000, 5000);
    if (!fetched(checksums) || checksums.text.empty() || checksums.text.back() != '\n') {
        spdlog::warn("Skin update ({}): could not fetch a complete checksums.sha256 (HTTP {}, curl error {}: {})",
                     skin_dir.filename().string(), checksums.status_code, static_cast<int>(checksums.error.code),
                     checksums.error.message);
        return false;
    }
    bool complete = true; // false once any listed file could not be fetched or written

    HashCache cache = load_hash_cache(skin_dir);
    HashCache seen; // this manifest's files: entries for files it no longer lists are dropped
    std::unordered_set<std::string> listed;
    std::istringstream lines(checksums.text);
    std::string hash, rel_path;
    int updated = 0;
    struct Job {
        std::string rel_path, hash;
        fs::path local_file;
        std::string key;
    };
    std::vector<Job> jobs;
    while (lines >> hash >> rel_path) {
        if (!rel_path.empty() && rel_path.front() == '*') {
            rel_path.erase(0, 1);
        }
        if (fs::path(rel_path).filename() == "checksums.sha256") {
            continue;
        }

        fs::path local_file = (skin_dir / rel_path).lexically_normal();
        const fs::path local_rel = local_file.lexically_relative(skin_dir);
        if (local_rel.empty() || *local_rel.begin() == ".." || *local_rel.begin() == kTempDirName) {
            spdlog::warn("Skin update ({}): skipping unsafe manifest path {}", skin_dir.filename().string(), rel_path);
            continue;
        }
        const std::string& key = rel_path; // the manifest's own spelling (UTF-8)
        listed.insert(key);
        uintmax_t size = 0;
        int64_t mtime = 0;
        if (file_stamp(local_file, size, mtime)) {
            // unchanged since its last check: the cached hash stands for the file
            auto cached = cache.find(key);
            std::string local_hash;
            if (cached != cache.end() && cached->second.size == size && cached->second.mtime == mtime) {
                local_hash = cached->second.hash;
            } else {
                local_hash = sha256_file(local_file);
            }
            if (!local_hash.empty()) {
                seen[key] = HashCacheEntry{local_hash, size, mtime};
            }
            if (local_hash == hash) {
                continue;
            }
        }

        // File is missing or differs: this pass has at least one real update.
        any_updated_.store(true);
        jobs.push_back({rel_path, hash, std::move(local_file), key});
    }

    set_progress(0, static_cast<int>(jobs.size()));
    std::mutex result_mutex;
    std::atomic<size_t> next_job{0};
    auto worker = [&] {
        PooledSession job_session;
        for (size_t i = next_job.fetch_add(1); i < jobs.size(); i = next_job.fetch_add(1)) {
            const Job& job = jobs[i];
            set_current_file(job.rel_path);

            std::string body;
            bool got = false;
            for (const std::string& url : {media_url(job.rel_path), raw_url(job.rel_path)}) {
                cpr::Response resp = get_url(*job_session, url, 60000, 5000);

                if (fetched(resp) && crypto::to_hex(crypto::sha256(resp.text)) == job.hash) {
                    body = std::move(resp.text);
                    got = true;
                    break;
                }
            }
            if (!got) {
                spdlog::warn(
                    "Skin update ({}): could not download {} matching checksums.sha256; keeping the local file",
                    skin_dir.filename().string(), job.rel_path);
                std::scoped_lock lock(result_mutex);
                complete = false;
                note_file_done();
                continue;
            }

            const fs::path tmp = temp_dir(skin_dir) / (std::to_string(i) + ".tmp");
            if (!write_atomically(tmp, job.local_file, body)) {
                spdlog::warn("Skin update ({}): failed to write {}; keeping the old file", skin_dir.filename().string(),
                             job.rel_path);
                std::scoped_lock lock(result_mutex);
                complete = false;
                note_file_done();
                continue;
            }
            uintmax_t size = 0;
            int64_t mtime = 0;
            {
                std::scoped_lock lock(result_mutex);
                if (file_stamp(job.local_file, size, mtime)) {
                    seen[job.key] = HashCacheEntry{job.hash, size, mtime};
                } else {
                    seen.erase(job.key);
                }
                ++updated;
            }
            note_file_updated();
            note_file_done();
        }
    };
    {
        constexpr size_t kConnections = 16;
        std::vector<std::thread> pool;
        const size_t n = std::min(kConnections, jobs.size());
        pool.reserve(n);
        for (size_t i = 0; i < n; i++) {
            pool.emplace_back(worker);
        }
        for (std::thread& t : pool) {
            t.join();
        }
    }

    int removed = 0;
    for (const auto& [rel, e] : cache) {
        if (listed.contains(rel)) {
            continue;
        }
        const fs::path local_file = (skin_dir / rel).lexically_normal();
        const fs::path local_rel = local_file.lexically_relative(skin_dir);
        if (local_rel.empty() || *local_rel.begin() == "..") {
            continue;
        }
        uintmax_t size = 0;
        int64_t mtime = 0;
        if (!file_stamp(local_file, size, mtime) || size != e.size || mtime != e.mtime) {
            continue;
        }
        std::error_code ec;
        if (!fs::remove(local_file, ec)) {
            continue;
        }
        ++removed;
        for (fs::path dir = local_file.parent_path(); dir != skin_dir && fs::is_empty(dir, ec) && !ec;
             dir = dir.parent_path()) {
            if (!fs::remove(dir, ec)) {
                break;
            }
        }
    }
    if (removed > 0) {
        any_updated_.store(true);
    }

    save_hash_cache(skin_dir, seen);
    fs::remove_all(temp_dir(skin_dir), tmp_ec);
    spdlog::info("Skin update ({}): {} file(s) updated, {} removed", skin_dir.filename().string(), updated, removed);
    return complete;
}

void SkinUpdater::scan_skins() {
    std::error_code ec;
    if (!fs::exists("Skins", ec)) {
        return;
    }
    for (const auto& entry : fs::directory_iterator("Skins", ec)) {
        if (ec || !entry.is_directory()) {
            continue;
        }

        std::ifstream repo_file(entry.path() / ".skin-repo");
        if (!repo_file) {
            continue;
        }
        std::string repo_url, branch;
        std::getline(repo_file, repo_url);
        std::getline(repo_file, branch);
        trim(repo_url);
        trim(branch);
        if (repo_url.empty()) {
            continue;
        }
        if (branch.empty()) {
            branch = "main";
        }

        update_one_skin(entry.path(), strip_dot_git(repo_url), branch);
    }
    session_pool.clear();
}

void SkinUpdater::start() {
    if (started_.exchange(true)) {
        return;
    }
    running_ = true;
    thread_ = std::thread([this] {
#ifdef _WIN32
        // below the game for CPU time: a pass without a hash cache reads every skin file
        win32_lower_thread_priority();
#endif
        try {
            scan_skins();
        } catch (const std::exception& e) {
            spdlog::error("Skin update: unhandled exception: {}", e.what());
        }
        running_ = false;
        std::scoped_lock lock(status_mutex_);
        current_skin_.clear();
        current_file_.clear();
        files_done_ = 0;
        files_total_ = 0;
    });
}

void SkinUpdater::download(std::vector<Request> requests) {
    if (thread_.joinable()) {
        thread_.join();
    }
    {
        std::scoped_lock lock(status_mutex_);
        failed_.clear();
    }
    started_ = true;
    running_ = true;
    thread_ = std::thread([this, requests = std::move(requests)] {
        for (const Request& r : requests) {
            const fs::path dir = fs::path("Skins") / r.name;
            std::error_code ec;
            fs::create_directories(dir, ec);
            if (!fs::exists(dir / ".skin-repo", ec)) {
                std::ofstream(dir / ".skin-repo") << r.url << '\n';
            }
            bool ok = false;
            try {
                ok = update_one_skin(dir, strip_dot_git(r.url), r.branch);
            } catch (const std::exception& e) {
                spdlog::error("Skin download ({}): {}", r.name, e.what());
            }
            if (!ok) {
                std::scoped_lock lock(status_mutex_);
                failed_.push_back(r.name);
            }
        }
        session_pool.clear();
        running_ = false;
        std::scoped_lock lock(status_mutex_);
        current_skin_.clear();
        current_file_.clear();
        files_done_ = 0;
        files_total_ = 0;
    });
}

bool SkinUpdater::in_progress() {
    return running_.load();
}

bool SkinUpdater::finished() {
    return started_.load() && !running_.load();
}

bool SkinUpdater::applying_updates() {
    return running_.load() && any_updated_.load();
}

bool SkinUpdater::updated_this_pass() {
    return any_updated_.load();
}

SkinUpdater::Status SkinUpdater::snapshot() {
    Status s;
    s.running = running_.load();
    s.finished = finished();
    s.any_updated = any_updated_.load();
    std::scoped_lock lock(status_mutex_);
    s.current_skin = current_skin_;
    s.current_file = current_file_;
    s.files_updated = files_updated_;
    s.files_done = files_done_;
    s.files_total = files_total_;
    s.failed = failed_;
    return s;
}

void SkinUpdater::shutdown() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

SkinUpdater skin_updater;

#else // __EMSCRIPTEN__

void SkinUpdater::start() {}
void SkinUpdater::download(std::vector<Request>) {}
bool SkinUpdater::in_progress() {
    return false;
}
bool SkinUpdater::finished() {
    return true;
}
bool SkinUpdater::applying_updates() {
    return false;
}
bool SkinUpdater::updated_this_pass() {
    return false;
}
SkinUpdater::Status SkinUpdater::snapshot() {
    Status s;
    s.finished = true;
    return s;
}
void SkinUpdater::shutdown() {}

SkinUpdater skin_updater;

#endif
