#include "online_songs.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <mutex>
#include <rapidjson/document.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <thread>
#include <unordered_map>

#include "../notify.h"

namespace fs = std::filesystem;

namespace online_songs {

    bool is_manifest_url(const std::string& entry) {
        return entry.starts_with("http://") || entry.starts_with("https://");
    }

    fs::path mirror_dir(const std::string& url) {
        std::string host = url.substr(url.find("://") + 3);
        host = host.substr(0, host.find_first_of("/?#"));
        std::ranges::replace(host, ':', '_');
        return fs::path("cache") / "songs" / host;
    }

} // namespace online_songs

#if defined(YATAIDON_HAS_HTTP)

#include <cpr/cpr.h>
#include <curl/curl.h>

#include "android_ca.h"

namespace online_songs {

    namespace {

        struct Mirror {
            fs::path dir;
            std::string files_url;
            std::unordered_map<std::string, std::string> files;
            std::unordered_map<std::string, std::array<std::string, 5>> hashes;
            std::unordered_map<std::string, std::string> local;
        };

        struct State {
            std::mutex mutex;
            std::vector<std::unique_ptr<Mirror>> mirrors;
        };

        State& state() {
            static State s;
            return s;
        }

        fs::path utf8_path(const std::string& s) {
            return fs::path(std::u8string(s.begin(), s.end()));
        }

        std::string url_encode(const std::string& path) {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string out;
            for (unsigned char c : path) {
                if (std::isalnum(c) || c == '/' || c == '-' || c == '.' || c == '_' || c == '~') {
                    out += static_cast<char>(c);
                } else {
                    out += '%';
                    out += hex[c >> 4];
                    out += hex[c & 15];
                }
            }
            return out;
        }

        void init_session(cpr::Session& session) {
#if defined(__ANDROID__)
            session.SetOption(android_ca());
            curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_SSL_EC_CURVES, "X25519:P-256:P-384");
#endif
            curl_easy_setopt(session.GetCurlHolder()->handle, CURLOPT_ACCEPT_ENCODING, "");
        }

        bool safe_relative(const std::string& rel) {
            const fs::path p = utf8_path(rel).lexically_normal();
            return !rel.empty() && p.is_relative() && *p.begin() != ".." && !p.filename().string().starts_with(".");
        }

        bool write_file(const fs::path& target, std::string_view body) {
            std::error_code ec;
            fs::create_directories(target.parent_path(), ec);
            fs::path tmp = target;
            tmp += ".part";
            {
                std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
                out << body;
                if (!out) {
                    fs::remove(tmp, ec);
                    return false;
                }
            }
            fs::rename(tmp, target, ec);
            return !ec;
        }

        bool download(cpr::Session& session, const std::string& url, const fs::path& target) {
            std::error_code ec;
            fs::create_directories(target.parent_path(), ec);
            fs::path tmp = target;
            tmp += ".part";
            bool ok = false;
            {
                std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
                if (out) {
                    session.SetUrl(cpr::Url{url});
                    session.SetTimeout(cpr::Timeout{300000});
                    session.SetConnectTimeout(cpr::ConnectTimeout{5000});
                    const cpr::Response r = session.Download(out);
                    out.close();
                    ok = r.status_code == 200 && r.error.code == cpr::ErrorCode::OK && out;
                }
            }
            if (ok) {
                fs::rename(tmp, target, ec);
                ok = !ec;
            }
            if (!ok) {
                fs::remove(tmp, ec);
                spdlog::warn("Online songs: could not download {}", url);
            }
            return ok;
        }

        std::string read_file(const fs::path& path) {
            std::ifstream in(path, std::ios::binary);
            return {std::istreambuf_iterator<char>(in), {}};
        }

        void load_downloaded(Mirror& m) {
            std::ifstream in(m.dir / ".downloaded");
            std::string line;
            while (std::getline(in, line)) {
                const size_t space = line.find(' ');
                if (space != std::string::npos) {
                    m.local[line.substr(space + 1)] = line.substr(0, space);
                }
            }
        }

        void save_downloaded(const Mirror& m) {
            std::string body;
            for (const auto& [rel, blob] : m.local) {
                body.append(blob).append(1, ' ').append(rel).append(1, '\n');
            }
            write_file(m.dir / ".downloaded", body);
        }

        bool on_disk(const Mirror& m, const std::string& rel) {
            std::error_code ec;
            return fs::exists(m.dir / utf8_path(rel), ec);
        }

        bool has_current(const Mirror& m, const std::string& rel, const std::string& blob) {
            auto it = m.local.find(rel);
            return it != m.local.end() && it->second == blob && on_disk(m, rel);
        }

        // false if any file failed
        template<typename OnDone>
        bool fetch_files(Mirror& m, const std::vector<std::pair<std::string, std::string>>& jobs, OnDone on_done) {
            std::mutex result_mutex;
            std::atomic<size_t> next{0};
            std::atomic<bool> complete{true};
            auto worker = [&] {
                cpr::Session session;
                init_session(session);
                for (size_t i = next++; i < jobs.size(); i = next++) {
                    const auto& [rel, blob] = jobs[i];
                    if (download(session, m.files_url + url_encode(rel), m.dir / utf8_path(rel))) {
                        std::scoped_lock lock(result_mutex);
                        m.local[rel] = blob;
                    } else {
                        complete = false;
                    }
                    on_done();
                }
            };
            {
                std::vector<std::jthread> pool;
                for (size_t i = 0; i < std::min<size_t>(16, jobs.size()); i++) {
                    pool.emplace_back(worker);
                }
            }
            save_downloaded(m);
            return complete;
        }

        // the mirror holding `chart` and the chart's path inside it
        std::pair<Mirror*, std::string> find_chart(const fs::path& chart) {
            const fs::path abs = fs::absolute(chart).lexically_normal();
            for (const auto& m : state().mirrors) {
                const fs::path rel = abs.lexically_relative(fs::absolute(m->dir).lexically_normal());
                if (!rel.empty() && *rel.begin() != "..") {
                    const auto u8 = rel.generic_u8string();
                    return {m.get(), std::string(u8.begin(), u8.end())};
                }
            }
            return {nullptr, {}};
        }

        void sync_one(const std::string& url, const fs::path& dir, std::atomic<float>& progress) {
            rapidjson::Document cached;
            const std::string cached_text = read_file(dir / ".manifest");
            cached.Parse(cached_text.c_str());
            const bool have_cached = !cached.HasParseError() && cached.IsObject() && cached.HasMember("files");

            cpr::Session session;
            init_session(session);
            session.SetUrl(cpr::Url{url});
            session.SetParameters(
                {{"since", have_cached && cached["version"].IsString() ? cached["version"].GetString() : ""}});
            session.SetTimeout(cpr::Timeout{120000});
            session.SetConnectTimeout(cpr::ConnectTimeout{5000});
            const cpr::Response r = session.Get();

            rapidjson::Document fresh;
            if (r.status_code == 200 && r.error.code == cpr::ErrorCode::OK) {
                fresh.Parse(r.text.c_str());
            }
            const bool got_full = !fresh.HasParseError() && fresh.IsObject() && fresh.HasMember("files");
            if (got_full) {
                write_file(dir / ".manifest", r.text);
            } else if (!have_cached) {
                spdlog::warn("Online songs: no manifest for {} (HTTP {})", url, r.status_code);
                return;
            } else if (!fresh.IsObject() || !fresh.HasMember("version")) {
                spdlog::warn("Online songs: could not fetch {} (HTTP {}), using the last copy", url, r.status_code);
            }
            const rapidjson::Document& doc = got_full ? fresh : cached;
            if (!doc["commit"].IsString() || !doc["repo"].IsString() || !doc["files"].IsObject() ||
                !doc["stubs"].IsObject() || !doc["hashes"].IsObject()) {
                spdlog::warn("Online songs: malformed manifest for {}", url);
                return;
            }

            auto m = std::make_unique<Mirror>();
            m->dir = dir;
            m->files_url = std::string(doc["repo"].GetString()) + "/media/commit/" + doc["commit"].GetString() + "/";
            for (const auto& f : doc["files"].GetObject()) {
                if (f.value.IsString() && safe_relative(f.name.GetString())) {
                    m->files.emplace(f.name.GetString(), f.value.GetString());
                }
            }
            for (const auto& h : doc["hashes"].GetObject()) {
                if (h.value.IsArray() && h.value.Size() == 5) {
                    auto& hashes = m->hashes[h.name.GetString()];
                    for (rapidjson::SizeType i = 0; i < 5; i++) {
                        hashes[i] = h.value[i].IsString() ? h.value[i].GetString() : "";
                    }
                }
            }
            load_downloaded(*m);

            int removed = 0;
            for (auto it = m->local.begin(); it != m->local.end();) {
                auto listed = m->files.find(it->first);
                std::string_view blob = it->second;
                if (blob.starts_with('~')) {
                    blob.remove_prefix(1);
                }
                if (listed != m->files.end() && listed->second == blob) {
                    ++it;
                    continue;
                }
                std::error_code ec;
                fs::remove(dir / utf8_path(it->first), ec);
                it = m->local.erase(it);
                removed++;
            }

            int stubs = 0;
            for (const auto& s : doc["stubs"].GetObject()) {
                const std::string rel = s.name.GetString();
                auto blob = m->files.find(rel);
                if (blob == m->files.end() || !s.value.IsString()) {
                    continue;
                }
                // a box.def comes whole; a chart is a stub until fetch_song brings the real one
                const bool chart = utf8_path(rel).extension() == ".tja" && m->hashes.contains(rel);
                const std::string marker = chart ? "~" + blob->second : blob->second;
                auto have = m->local.find(rel);
                if (have != m->local.end() && (have->second == marker || have->second == blob->second) &&
                    on_disk(*m, rel)) {
                    continue;
                }
                if (write_file(dir / utf8_path(rel), {s.value.GetString(), s.value.GetStringLength()})) {
                    m->local[rel] = marker;
                    stubs++;
                }
            }

            // charts the server sent no stub for are fetched whole so song select still lists them
            std::vector<std::pair<std::string, std::string>> jobs;
            for (const auto& [rel, blob] : m->files) {
                const fs::path ext = utf8_path(rel).extension();
                if ((ext == ".tja" || ext == ".def") && !m->local.contains(rel)) {
                    jobs.emplace_back(rel, blob);
                }
            }
            std::atomic<size_t> done{0};
            fetch_files(*m, jobs, [&] { progress = static_cast<float>(++done) / jobs.size(); });
            spdlog::info("Online songs ({}): {} files listed, {} stubs written, {} downloaded, {} removed", url,
                         m->files.size(), stubs, jobs.size(), removed);
            state().mirrors.push_back(std::move(m));
        }

    } // namespace

    void sync(const Manifests& manifests, std::atomic<float>& progress) {
        std::scoped_lock lock(state().mutex);
        for (const auto& [url, dir] : manifests) {
            if (std::ranges::contains(state().mirrors, dir, &Mirror::dir)) {
                continue;
            }
            progress = 0.0f;
            try {
                sync_one(url, dir, progress);
            } catch (const std::exception& e) {
                spdlog::error("Online songs ({}): {}", url, e.what());
            }
        }
    }

    std::optional<std::array<std::string, 5>> chart_hashes(const fs::path& chart) {
        std::scoped_lock lock(state().mutex);
        auto [m, rel] = find_chart(chart);
        if (!m) {
            return std::nullopt;
        }
        auto it = m->hashes.find(rel);
        return it != m->hashes.end() ? std::optional(it->second) : std::nullopt;
    }

    bool fetch_song(const fs::path& chart) {
        std::scoped_lock lock(state().mutex);
        auto [m, chart_rel] = find_chart(chart);
        if (!m) {
            return true;
        }
        const size_t slash = chart_rel.rfind('/');
        const std::string prefix = slash == std::string::npos ? "" : chart_rel.substr(0, slash + 1);
        std::vector<std::pair<std::string, std::string>> jobs;
        for (const auto& [rel, blob] : m->files) {
            if (rel.starts_with(prefix) && rel.find('/', prefix.size()) == std::string::npos &&
                !has_current(*m, rel, blob)) {
                jobs.emplace_back(rel, blob);
            }
        }
        if (jobs.empty()) {
            return true;
        }
        spdlog::info("Online songs: downloading {} file(s) for {}", jobs.size(), chart.string());
        notify("Downloading song");
        return fetch_files(*m, jobs, [] {});
    }

} // namespace online_songs

#else // no HTTP client

namespace online_songs {

    void sync(const Manifests&, std::atomic<float>&) {}

    std::optional<std::array<std::string, 5>> chart_hashes(const fs::path&) {
        return std::nullopt;
    }

    bool fetch_song(const fs::path&) {
        return true;
    }

} // namespace online_songs

#endif
