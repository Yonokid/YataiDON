#include "network.h"
#include <thread>
#include "scores.h"
#include "color_utils.h"
#include "global_data.h"
#include "sha256.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <spdlog/spdlog.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <map>
#include <random>

#if defined(NETWORK_ENABLED) && defined(__ANDROID__)
#include <SDL3/SDL.h>
#include <fstream>
#endif

NetworkClient network;

std::string modifiers_to_json(const Modifiers& m) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    doc.AddMember("auto_play", m.auto_play, allocator);
    doc.AddMember("speed", m.speed, allocator);
    doc.AddMember("display", m.display, allocator);
    doc.AddMember("inverse", m.inverse, allocator);
    doc.AddMember("random", m.random, allocator);
    doc.AddMember("subdiff", m.subdiff, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}

#if defined(NETWORK_ENABLED)

namespace {

// Compares dotted version strings numerically component by component (e.g. "1.2.0" < "1.10.0").
bool version_less(const std::string& a, const std::string& b) {
    size_t ai = 0, bi = 0;
    while (ai < a.size() || bi < b.size()) {
        int an = 0, bn = 0;
        while (ai < a.size() && a[ai] != '.') an = an * 10 + (a[ai++] - '0');
        while (bi < b.size() && b[bi] != '.') bn = bn * 10 + (b[bi++] - '0');
        if (an != bn) return an < bn;
        if (ai < a.size()) ai++;
        if (bi < b.size()) bi++;
    }
    return false;
}

template <std::size_t N>
struct ObfuscatedString {
    std::array<char, N> data{};

    constexpr ObfuscatedString(const char (&str)[N]) {
        for (std::size_t i = 0; i < N; ++i) data[i] = str[i] ^ key(i);
    }

    static constexpr char key(std::size_t i) {
        constexpr char seed[] = __TIME__;
        return seed[i % (sizeof(seed) - 1)] ^ static_cast<char>(i * 41 + 7);
    }

    std::string decode() const {
        std::string out(N - 1, '\0');
        for (std::size_t i = 0; i < N - 1; ++i) out[i] = data[i] ^ key(i);
        return out;
    }
};

std::string secret_key() {
    static constexpr ObfuscatedString obfuscated_key(NETWORK_AUTH_KEY);
    static const std::string key = obfuscated_key.decode();
    return key;
}

std::string random_nonce() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                  static_cast<unsigned long long>(dist(rng)),
                  static_cast<unsigned long long>(dist(rng)));
    return std::string(buf);
}

std::string current_timestamp() {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
    return std::to_string(secs.count());
}

cpr::Header signed_headers(const std::string& method, const std::string& path,
                            const std::map<std::string, std::string>& params) {
    std::string timestamp = current_timestamp();
    std::string nonce = random_nonce();

    std::string canonical = method + "\n" + path + "\n";
    for (const auto& [k, v] : params) canonical += k + "=" + v + "&";
    canonical += "\n" + timestamp + "\n" + nonce;

    std::string signature = crypto::to_hex(crypto::hmac_sha256(secret_key(), canonical));

    return cpr::Header{
        {"X-Timestamp", timestamp},
        {"X-Nonce", nonce},
        {"X-Signature", signature},
        {"X-Client-Version", CLIENT_VERSION},
    };
}

#if defined(__ANDROID__)
// libcurl on Android has no system CA store to fall back on; extract the
// bundled Mozilla cacert.pem (android/app/src/main/assets/cacert.pem) to a
// real path once and point every request's CURLOPT_CAINFO at it.
std::string ca_bundle_path() {
    static const std::string path = [] {
        const std::string out_path = "/sdcard/YataiDON/cacert.pem";
        std::ifstream existing(out_path, std::ios::binary);
        if (existing.good()) return out_path;

        SDL_IOStream* io = SDL_IOFromFile("cacert.pem", "r");
        if (!io) {
            spdlog::error("Failed to open bundled cacert.pem asset");
            return std::string{};
        }
        Sint64 size = SDL_GetIOSize(io);
        if (size <= 0) {
            SDL_CloseIO(io);
            return std::string{};
        }
        std::string buf(static_cast<std::size_t>(size), '\0');
        SDL_ReadIO(io, buf.data(), static_cast<std::size_t>(size));
        SDL_CloseIO(io);

        std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            spdlog::error("Failed to write cacert.pem to {}", out_path);
            return std::string{};
        }
        out << buf;
        return out_path;
    }();
    return path;
}

cpr::SslOptions android_ca() {
    return cpr::Ssl(cpr::ssl::CaInfo{ca_bundle_path()});
}
#define NETWORK_CA_OPT , android_ca()
#else
#define NETWORK_CA_OPT
#endif

}  // namespace

static bool network_enabled() {
    return global_data.config && global_data.config->network.online_play;
}

static std::string network_url(const std::string& endpoint) {
    std::string base = NETWORK_URL;
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + endpoint;
}

bool NetworkClient::probe_online() {
    if (!network_enabled()) { online = false; return false; }
    cpr::Response r = cpr::Get(
        cpr::Url{network_url("/health")},
        signed_headers("GET", "/health", {}),
        cpr::Timeout{1500}
        NETWORK_CA_OPT
    );
    online = (r.status_code == 200);
    if (!online) spdlog::warn("Network: server unreachable at startup (HTTP {}), skipping profile sync", r.status_code);
    return online;
}

void NetworkClient::check_heartbeat() {
    if (pending_heartbeat.has_value()) return;
    pending_heartbeat = cpr::GetAsync(
        cpr::Url{network_url("/health")},
        signed_headers("GET", "/health", {}),
        cpr::Timeout{2000}
        NETWORK_CA_OPT
    );
}

bool NetworkClient::check_import_requested(const std::string& access_code) {
    if (!network_enabled()) return false;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return false;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return false;
    return doc.HasMember("import_requested") && doc["import_requested"].IsBool()
        && doc["import_requested"].GetBool();
}

bool NetworkClient::fetch_chara_colors(const std::string& access_code, ray::Color& color_1, ray::Color& color_2, ray::Color& color_3) {
    if (!network_enabled()) return false;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return false;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return false;
    if (!doc.HasMember("chara_color_1") || !doc["chara_color_1"].IsString()) return false;
    if (!doc.HasMember("chara_color_2") || !doc["chara_color_2"].IsString()) return false;
    if (!doc.HasMember("chara_color_3") || !doc["chara_color_3"].IsString()) return false;

    try {
        color_1 = parse_hex_color(doc["chara_color_1"].GetString());
        color_2 = parse_hex_color(doc["chara_color_2"].GetString());
        color_3 = parse_hex_color(doc["chara_color_3"].GetString());
    } catch (const std::invalid_argument&) {
        return false;
    }
    return true;
}

bool NetworkClient::fetch_username(const std::string& access_code, std::string& username) {
    if (!network_enabled()) return false;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return false;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return false;
    if (!doc.HasMember("username") || !doc["username"].IsString()) return false;

    username = doc["username"].GetString();
    return true;
}

bool NetworkClient::fetch_title(const std::string& access_code, std::string& title) {
    if (!network_enabled()) return false;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return false;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return false;
    if (!doc.HasMember("title") || !doc["title"].IsString()) return false;

    title = doc["title"].GetString();
    return true;
}

bool NetworkClient::fetch_title_bg(const std::string& access_code, int& title_bg) {
    if (!network_enabled()) return false;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return false;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return false;
    if (!doc.HasMember("title_bg") || !doc["title_bg"].IsInt()) return false;

    title_bg = doc["title_bg"].GetInt();
    return true;
}

void NetworkClient::update_username(const std::string& access_code, const std::string& username) {
    if (!network_enabled()) return;
    cpr::Response response = cpr::Post(
        cpr::Url{network_url("/update_username")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Payload{{"username", username}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) {
        spdlog::error("Failed to update username: HTTP {} - {}", response.status_code, response.text);
    }
}

bool NetworkClient::fetch_costume(const std::string& access_code, int& head_index, int& body_index, int& cos_index, bool& is_costume) {
    if (!network_enabled()) return false;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return false;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return false;
    if (!doc.HasMember("chara_head_index") || !doc["chara_head_index"].IsInt()) return false;
    if (!doc.HasMember("chara_body_index") || !doc["chara_body_index"].IsInt()) return false;
    if (!doc.HasMember("chara_cos_index") || !doc["chara_cos_index"].IsInt()) return false;
    if (!doc.HasMember("chara_is_costume") || !doc["chara_is_costume"].IsBool()) return false;

    head_index = doc["chara_head_index"].GetInt();
    body_index = doc["chara_body_index"].GetInt();
    cos_index = doc["chara_cos_index"].GetInt();
    is_costume = doc["chara_is_costume"].GetBool();
    return true;
}

void NetworkClient::update_costume(const std::string& access_code, int head_index, int body_index, int cos_index, bool is_costume) {
    if (!network_enabled()) return;
    cpr::Response response = cpr::Post(
        cpr::Url{network_url("/update_costume")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Payload{
            {"chara_head_index", std::to_string(head_index)},
            {"chara_body_index", std::to_string(body_index)},
            {"chara_cos_index", std::to_string(cos_index)},
            {"chara_is_costume", is_costume ? "true" : "false"},
        },
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) {
        spdlog::error("Failed to update costume: HTTP {} - {}", response.status_code, response.text);
    }
}

std::vector<RemoteScore> NetworkClient::fetch_scores(const std::string& access_code) {
    std::vector<RemoteScore> result;
    if (!network_enabled()) return result;
    cpr::Response response = cpr::Get(
        cpr::Url{network_url("/user")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{10000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) return result;

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) return result;
    if (!doc.HasMember("scores") || !doc["scores"].IsArray()) return result;

    for (auto& s : doc["scores"].GetArray()) {
        if (!s.IsObject()) continue;
        if (!s.HasMember("hash") || !s["hash"].IsString()) continue;
        if (!s.HasMember("difficulty") || !s["difficulty"].IsInt()) continue;

        RemoteScore rs;
        rs.hash = s["hash"].GetString();
        rs.difficulty = s["difficulty"].GetInt();
        rs.score.score     = s.HasMember("score")     && s["score"].IsInt()     ? s["score"].GetInt()     : 0;
        rs.score.good      = s.HasMember("good")      && s["good"].IsInt()      ? s["good"].GetInt()      : 0;
        rs.score.ok        = s.HasMember("ok")        && s["ok"].IsInt()        ? s["ok"].GetInt()        : 0;
        rs.score.bad       = s.HasMember("bad")       && s["bad"].IsInt()       ? s["bad"].GetInt()       : 0;
        rs.score.drumroll  = s.HasMember("drumroll")  && s["drumroll"].IsInt()  ? s["drumroll"].GetInt()  : 0;
        rs.score.max_combo = s.HasMember("max_combo") && s["max_combo"].IsInt() ? s["max_combo"].GetInt() : 0;
        rs.score.crown = static_cast<Crown>(s.HasMember("crown") && s["crown"].IsInt() ? s["crown"].GetInt() : 0);
        rs.score.rank  = static_cast<Rank>(s.HasMember("rank")   && s["rank"].IsInt()   ? s["rank"].GetInt()   : 0);
        result.push_back(std::move(rs));
    }
    return result;
}

void NetworkClient::clear_import_flag(const std::string& access_code) {
    if (!network_enabled()) return;
    cpr::Response response = cpr::Post(
        cpr::Url{network_url("/clear_import_flag")},
        signed_headers("POST", "/clear_import_flag", {{"access_code", access_code}}),
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) {
        spdlog::error("Failed to clear import flag: HTTP {} - {}", response.status_code, response.text);
    }
}

std::string NetworkClient::register_user(const std::string& username) {
    if (!network_enabled()) return "";
    cpr::Response response = cpr::Post(
        cpr::Url{network_url("/register_user")},
        signed_headers("POST", "/register_user", {{"username", username}}),
        cpr::Payload{{"username", username}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
    if (response.status_code != 200) {
        spdlog::error("Failed to register user: HTTP {} - {}", response.status_code, response.text);
        return "";
    }
    return response.text;
}

std::string NetworkClient::map_to_json(const std::map<double, InputLogType>& my_map) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    for (const auto& pair : my_map) {
        rapidjson::Value key(std::to_string(pair.first).c_str(), allocator);
        doc.AddMember(key, (int)pair.second, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}

void NetworkClient::submit_score(std::string& hash, int difficulty, const std::string& access_code, Score score, std::map<double, InputLogType> input_log, int64_t played_at, const std::string& modifiers_json, bool chara_is_costume, int chara_cos_index) {
    if (!network_enabled()) return;
    std::map<std::string, std::string> params{
        {"access_code", access_code},
        {"hash", hash},
        {"difficulty", std::to_string(difficulty)},
        {"crown", std::to_string(static_cast<int>(score.crown))},
        {"rank", std::to_string(static_cast<int>(score.rank))},
        {"score", std::to_string(score.score)},
        {"good", std::to_string(score.good)},
        {"ok", std::to_string(score.ok)},
        {"bad", std::to_string(score.bad)},
        {"drumroll", std::to_string(score.drumroll)},
        {"max_combo", std::to_string(score.max_combo)},
    };
    // The upload runs off the render thread: a synchronous POST stalled the end of
    // the song for up to the 5 s timeout whenever the server was unreachable.
    std::thread([this, params = std::move(params), input_log = std::move(input_log), played_at,
                 modifiers_json, chara_is_costume, chara_cos_index]() mutable {
        cpr::Response response = cpr::Post(
            cpr::Url{network_url("/submit_score")},
            signed_headers("POST", "/submit_score", params),
            cpr::Parameters{
                {"access_code", params["access_code"]},
                {"hash", params["hash"]},
                {"difficulty", params["difficulty"]},
                {"crown", params["crown"]},
                {"rank", params["rank"]},
                {"score", params["score"]},
                {"good", params["good"]},
                {"ok", params["ok"]},
                {"bad", params["bad"]},
                {"drumroll", params["drumroll"]},
                {"max_combo", params["max_combo"]},
            },
            cpr::Payload{
                {"input_log", map_to_json(input_log)},
                {"played_at", played_at > 0 ? std::to_string(played_at) : ""},
                {"modifiers", modifiers_json},
                {"chara_is_costume", chara_is_costume ? "true" : "false"},
                {"chara_cos_index", std::to_string(chara_cos_index)},
            },
            cpr::Timeout{5000}
            NETWORK_CA_OPT
        );
        if (response.status_code != 200) {
            spdlog::error("Failed to submit score: HTTP {} - {}", response.status_code, response.text);
        }
    }).detach();
}

void NetworkClient::poll_song_jump(const std::string& access_code) {
    if (!network_enabled()) return;
    if (pending_song_jump.has_value()) return;
    pending_song_jump = cpr::GetAsync(
        cpr::Url{network_url("/poll_song_jump")},
        cpr::Parameters{{"access_code", access_code}},
        cpr::Timeout{5000}
        NETWORK_CA_OPT
    );
}

std::optional<std::string> NetworkClient::take_song_jump_result() {
    if (!song_jump_result.has_value()) return std::nullopt;
    std::optional<std::string> result = std::move(song_jump_result);
    song_jump_result.reset();
    return result;
}

void NetworkClient::update(double current_ms) {
    if (!network_enabled()) {
        online = false;
        return;
    }
    if (current_ms - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
        last_heartbeat_ms = current_ms;
        check_heartbeat();
    }

    if (pending_heartbeat.has_value() &&
        pending_heartbeat->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_heartbeat->get();
        pending_heartbeat.reset();

        bool was_online = online;
        online = response.status_code == 200;
        if (online != was_online) {
            spdlog::info("hiroba heartbeat: {}", online ? "online" : "offline");
        }

        if (online) {
            rapidjson::Document doc;
            if (!doc.Parse(response.text.c_str()).HasParseError() &&
                doc.HasMember("min_client_version") && doc["min_client_version"].IsString()) {
                outdated = version_less(CLIENT_VERSION, doc["min_client_version"].GetString());
            }
        }
    }

    if (pending_song_jump.has_value() &&
        pending_song_jump->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_song_jump->get();
        pending_song_jump.reset();

        if (response.status_code == 200) {
            rapidjson::Document doc;
            if (!doc.Parse(response.text.c_str()).HasParseError() &&
                doc.HasMember("hash") && doc["hash"].IsString()) {
                song_jump_result = doc["hash"].GetString();
            }
        }
    }
}

#else

bool NetworkClient::probe_online() { return false; }
std::string NetworkClient::register_user(const std::string&) { return ""; }
void NetworkClient::submit_score(std::string&, int, const std::string&, Score, std::map<double, InputLogType> input_log, int64_t, const std::string&, bool, int) {}
bool NetworkClient::check_import_requested(const std::string&) { return false; }
void NetworkClient::clear_import_flag(const std::string&) {}
bool NetworkClient::fetch_chara_colors(const std::string&, ray::Color&, ray::Color&, ray::Color&) { return false; }
bool NetworkClient::fetch_username(const std::string&, std::string&) { return false; }
void NetworkClient::update_username(const std::string&, const std::string&) {}
bool NetworkClient::fetch_title(const std::string&, std::string&) { return false; }
bool NetworkClient::fetch_title_bg(const std::string&, int&) { return false; }
bool NetworkClient::fetch_costume(const std::string&, int&, int&, int&, bool&) { return false; }
void NetworkClient::update_costume(const std::string&, int, int, int, bool) {}
std::vector<RemoteScore> NetworkClient::fetch_scores(const std::string&) { return {}; }
void NetworkClient::poll_song_jump(const std::string&) {}
std::optional<std::string> NetworkClient::take_song_jump_result() { return std::nullopt; }
void NetworkClient::update(double) {}

#endif
