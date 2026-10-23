#include "network.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <map>
#include <random>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <spdlog/spdlog.h>

#include "android_ca.h"
#include "color_utils.h"
#include "global_data.h"
#include "parsers/tja.h"
#include "scores.h"
#include "sha256.h"

#if defined(NETWORK_ENABLED) && defined(__ANDROID__)
#include <fstream>
#include <jni.h>
#include <sstream>
#include <vector>

#include "sdl_compat.h"
#endif

#ifdef GetObject
#undef GetObject
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
            long long an = 0, bn = 0;
            while (ai < a.size() && a[ai] != '.') {
                if (!std::isdigit(static_cast<unsigned char>(a[ai]))) {
                    return false; // malformed: treat as not-less
                }
                an = std::min<long long>(an * 10 + (a[ai++] - '0'), 1'000'000'000LL);
            }
            while (bi < b.size() && b[bi] != '.') {
                if (!std::isdigit(static_cast<unsigned char>(b[bi]))) {
                    return false;
                }
                bn = std::min<long long>(bn * 10 + (b[bi++] - '0'), 1'000'000'000LL);
            }
            if (an != bn) {
                return an < bn;
            }
            if (ai < a.size()) {
                ai++;
            }
            if (bi < b.size()) {
                bi++;
            }
        }
        return false;
    }

    template<std::size_t N>
    struct ObfuscatedString {
        std::array<char, N> data{};

        constexpr ObfuscatedString(const char (&str)[N]) {
            for (std::size_t i = 0; i < N; ++i) {
                data[i] = str[i] ^ key(i);
            }
        }

        static constexpr char key(std::size_t i) {
            constexpr char seed[] = __TIME__;
            return seed[i % (sizeof(seed) - 1)] ^ static_cast<char>(i * 41 + 7);
        }

        std::string decode() const {
            std::string out(N - 1, '\0');
            for (std::size_t i = 0; i < N - 1; ++i) {
                out[i] = data[i] ^ key(i);
            }
            return out;
        }
    };

    std::string secret_key() {
        if (global_data.config && !global_data.config->network.api_key.empty()) {
            return global_data.config->network.api_key;
        }

#if defined(NETWORK_AUTH_KEY)
        static constexpr ObfuscatedString obfuscated_key(NETWORK_AUTH_KEY);
        static const std::string key = obfuscated_key.decode();
        return key;
#else
        return {};
#endif
    }

    std::string random_nonce() {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint64_t> dist;
        char buf[33];
        std::snprintf(buf, sizeof(buf), "%016llx%016llx", static_cast<unsigned long long>(dist(rng)),
                      static_cast<unsigned long long>(dist(rng)));
        return std::string(buf);
    }

    std::string current_timestamp() {
        auto secs =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
        return std::to_string(secs.count());
    }

    cpr::Header signed_headers(const std::string& method, const std::string& path,
                               const std::map<std::string, std::string>& params) {
        std::string timestamp = current_timestamp();
        std::string nonce = random_nonce();

        std::string canonical = method + "\n" + path + "\n";
        for (const auto& [k, v] : params) {
            canonical += std::format("{}={}&", k, v);
        }
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
#define NETWORK_CA_OPT , android_ca()

    void install_apk(const std::string& path) {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (!env || !activity) {
            spdlog::error("Update: could not get JNI env/activity");
            return;
        }
        jclass clazz = env->GetObjectClass(activity);
        jmethodID mid = env->GetMethodID(clazz, "installApk", "(Ljava/lang/String;)V");
        if (!mid) {
            spdlog::error("Update: YataiDONActivity.installApk(String) not found");
            env->ExceptionClear();
            env->DeleteLocalRef(clazz);
            env->DeleteLocalRef(activity);
            return;
        }
        jstring jpath = env->NewStringUTF(path.c_str());
        env->CallVoidMethod(activity, mid, jpath);
        env->DeleteLocalRef(jpath);
        env->DeleteLocalRef(clazz);
        env->DeleteLocalRef(activity);
    }
#else
#define NETWORK_CA_OPT
#endif

} // namespace

static bool network_enabled() {
    return global_data.config && global_data.config->network.online_play && !secret_key().empty();
}

static std::string network_url(const std::string& endpoint) {
    std::string base = NETWORK_URL;
    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + endpoint;
}

bool NetworkClient::probe_online() {
    if (!network_enabled()) {
        online = false;
        return false;
    }
    cpr::Response r = cpr::Get(cpr::Url{network_url("/health")}, signed_headers("GET", "/health", {}),
                               cpr::Timeout{1500} NETWORK_CA_OPT);
    online = (r.status_code == 200);
    if (!online) {
        spdlog::warn("Network: server unreachable at startup (HTTP {}), skipping profile sync", r.status_code);
    }
    return online;
}

void NetworkClient::start_probe() {
    if (!network_enabled()) {
        online = false;
        return;
    }
    probing_ = true;
    check_heartbeat();
}

void NetworkClient::check_heartbeat() {
    if (pending_heartbeat.has_value()) {
        return;
    }
    pending_heartbeat = cpr::GetAsync(cpr::Url{network_url("/health")}, signed_headers("GET", "/health", {}),
                                      cpr::Timeout{2000} NETWORK_CA_OPT);
}

bool NetworkClient::check_import_requested(const std::string& access_code) {
    if (!network_enabled()) {
        return false;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return false;
    }
    return doc.HasMember("import_requested") && doc["import_requested"].IsBool() && doc["import_requested"].GetBool();
}

bool NetworkClient::fetch_chara_colors(const std::string& access_code, Color& color_1, Color& color_2, Color& color_3) {
    if (!network_enabled()) {
        return false;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return false;
    }
    if (!doc.HasMember("chara_color_1") || !doc["chara_color_1"].IsString()) {
        return false;
    }
    if (!doc.HasMember("chara_color_2") || !doc["chara_color_2"].IsString()) {
        return false;
    }
    if (!doc.HasMember("chara_color_3") || !doc["chara_color_3"].IsString()) {
        return false;
    }

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
    if (!network_enabled()) {
        return false;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return false;
    }
    if (!doc.HasMember("username") || !doc["username"].IsString()) {
        return false;
    }

    username = doc["username"].GetString();
    return true;
}

bool NetworkClient::fetch_title(const std::string& access_code, std::string& title) {
    if (!network_enabled()) {
        return false;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return false;
    }
    if (!doc.HasMember("title") || !doc["title"].IsString()) {
        return false;
    }

    title = doc["title"].GetString();
    return true;
}

bool NetworkClient::fetch_title_bg(const std::string& access_code, int& title_bg) {
    if (!network_enabled()) {
        return false;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return false;
    }
    if (!doc.HasMember("title_bg") || !doc["title_bg"].IsInt()) {
        return false;
    }

    title_bg = doc["title_bg"].GetInt();
    return true;
}

void NetworkClient::update_username(const std::string& access_code, const std::string& username) {
    if (!network_enabled()) {
        return;
    }
    cpr::Response response =
        cpr::Post(cpr::Url{network_url("/update_username")},
                  signed_headers("POST", "/update_username", {{"access_code", access_code}, {"username", username}}),
                  cpr::Parameters{{"access_code", access_code}}, cpr::Payload{{"username", username}},
                  cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        spdlog::error("Failed to update username: HTTP {} - {}", response.status_code, response.text);
    }
}

bool NetworkClient::fetch_costume(const std::string& access_code, int& head_index, int& body_index, int& cos_index,
                                  bool& is_costume) {
    if (!network_enabled()) {
        return false;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return false;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return false;
    }
    if (!doc.HasMember("chara_head_index") || !doc["chara_head_index"].IsInt()) {
        return false;
    }
    if (!doc.HasMember("chara_body_index") || !doc["chara_body_index"].IsInt()) {
        return false;
    }
    if (!doc.HasMember("chara_cos_index") || !doc["chara_cos_index"].IsInt()) {
        return false;
    }
    if (!doc.HasMember("chara_is_costume") || !doc["chara_is_costume"].IsBool()) {
        return false;
    }

    head_index = doc["chara_head_index"].GetInt();
    body_index = doc["chara_body_index"].GetInt();
    cos_index = doc["chara_cos_index"].GetInt();
    is_costume = doc["chara_is_costume"].GetBool();
    return true;
}

std::optional<RemoteUser> NetworkClient::fetch_user(const std::string& access_code) {
    if (!network_enabled()) {
        return std::nullopt;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return std::nullopt;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError() || !doc.IsObject()) {
        return std::nullopt;
    }

    RemoteUser user;
    if (doc.HasMember("username") && doc["username"].IsString()) {
        user.username = doc["username"].GetString();
    }
    if (doc.HasMember("title") && doc["title"].IsString()) {
        user.title = doc["title"].GetString();
    }
    if (doc.HasMember("title_bg") && doc["title_bg"].IsInt()) {
        user.title_bg = doc["title_bg"].GetInt();
    }
    if (doc.HasMember("chara_color_1") && doc["chara_color_1"].IsString() && doc.HasMember("chara_color_2") &&
        doc["chara_color_2"].IsString() && doc.HasMember("chara_color_3") && doc["chara_color_3"].IsString()) {
        try {
            user.chara_colors = std::array<Color, 3>{parse_hex_color(doc["chara_color_1"].GetString()),
                                                     parse_hex_color(doc["chara_color_2"].GetString()),
                                                     parse_hex_color(doc["chara_color_3"].GetString())};
        } catch (const std::invalid_argument&) {
        }
    }
    if (doc.HasMember("chara_head_index") && doc["chara_head_index"].IsInt() && doc.HasMember("chara_body_index") &&
        doc["chara_body_index"].IsInt() && doc.HasMember("chara_cos_index") && doc["chara_cos_index"].IsInt() &&
        doc.HasMember("chara_is_costume") && doc["chara_is_costume"].IsBool()) {
        user.costume = RemoteUser::Costume{doc["chara_head_index"].GetInt(), doc["chara_body_index"].GetInt(),
                                           doc["chara_cos_index"].GetInt(), doc["chara_is_costume"].GetBool()};
    }
    user.import_requested =
        doc.HasMember("import_requested") && doc["import_requested"].IsBool() && doc["import_requested"].GetBool();
    return user;
}

#if defined(__ANDROID__)
namespace {
    constexpr char kUpdateChecksumUrl[] =
        "https://github.com/yonokid/YataiDON/releases/latest/download/checksums-android.sha256";
    constexpr char kUpdateApkUrl[] =
        "https://github.com/yonokid/YataiDON/releases/latest/download/YataiDON-Android-Update.apk";
    constexpr char kUpdateMarkerPath[] = "update_apk.sha256";
    constexpr char kUpdateApkPath[] = "/sdcard/YataiDON/update.apk";
    constexpr char kUpdateApkTmpPath[] = "/sdcard/YataiDON/update.apk.part";

    std::optional<std::string> sha256_of_file(const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return std::nullopt;
        }
        crypto::Sha256 hasher;
        std::vector<char> buf(1 << 16);
        while (in.read(buf.data(), static_cast<std::streamsize>(buf.size())) || in.gcount() > 0) {
            hasher.update(reinterpret_cast<const uint8_t*>(buf.data()), static_cast<std::size_t>(in.gcount()));
        }
        return crypto::to_hex(hasher.finalize());
    }
} // namespace

void NetworkClient::check_and_install_android_update() {
    if (android_update_checked) {
        return;
    }
    android_update_checked = true;
    pending_update_checksum = cpr::GetAsync(cpr::Url{kUpdateChecksumUrl}, cpr::Timeout{5000} NETWORK_CA_OPT);
}
#else
void NetworkClient::check_and_install_android_update() {}
#endif

void NetworkClient::update_costume(const std::string& access_code, int head_index, int body_index, int cos_index,
                                   bool is_costume) {
    if (!network_enabled()) {
        return;
    }
    cpr::Response response = cpr::Post(cpr::Url{network_url("/update_costume")},
                                       signed_headers("POST", "/update_costume",
                                                      {
                                                          {"access_code", access_code},
                                                          {"chara_head_index", std::to_string(head_index)},
                                                          {"chara_body_index", std::to_string(body_index)},
                                                          {"chara_cos_index", std::to_string(cos_index)},
                                                          {"chara_is_costume", is_costume ? "true" : "false"},
                                                      }),
                                       cpr::Parameters{{"access_code", access_code}},
                                       cpr::Payload{
                                           {"chara_head_index", std::to_string(head_index)},
                                           {"chara_body_index", std::to_string(body_index)},
                                           {"chara_cos_index", std::to_string(cos_index)},
                                           {"chara_is_costume", is_costume ? "true" : "false"},
                                       },
                                       cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        spdlog::error("Failed to update costume: HTTP {} - {}", response.status_code, response.text);
    }
}

std::vector<RemoteScore> NetworkClient::fetch_scores(const std::string& access_code) {
    std::vector<RemoteScore> result;
    if (!network_enabled()) {
        return result;
    }
    cpr::Response response =
        cpr::Get(cpr::Url{network_url("/user")}, signed_headers("GET", "/user", {{"access_code", access_code}}),
                 cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{10000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        return result;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError()) {
        return result;
    }
    if (!doc.HasMember("scores") || !doc["scores"].IsArray()) {
        return result;
    }

    for (auto& s : doc["scores"].GetArray()) {
        if (!s.IsObject()) {
            continue;
        }
        if (!s.HasMember("hash") || !s["hash"].IsString()) {
            continue;
        }
        if (!s.HasMember("difficulty") || !s["difficulty"].IsInt()) {
            continue;
        }

        RemoteScore rs;
        rs.hash = s["hash"].GetString();
        rs.difficulty = s["difficulty"].GetInt();
        rs.score.score = s.HasMember("score") && s["score"].IsInt() ? s["score"].GetInt() : 0;
        rs.score.good = s.HasMember("good") && s["good"].IsInt() ? s["good"].GetInt() : 0;
        rs.score.ok = s.HasMember("ok") && s["ok"].IsInt() ? s["ok"].GetInt() : 0;
        rs.score.bad = s.HasMember("bad") && s["bad"].IsInt() ? s["bad"].GetInt() : 0;
        rs.score.drumroll = s.HasMember("drumroll") && s["drumroll"].IsInt() ? s["drumroll"].GetInt() : 0;
        rs.score.max_combo = s.HasMember("max_combo") && s["max_combo"].IsInt() ? s["max_combo"].GetInt() : 0;
        rs.score.crown = static_cast<Crown>(s.HasMember("crown") && s["crown"].IsInt() ? s["crown"].GetInt() : 0);
        rs.score.rank = static_cast<Rank>(s.HasMember("rank") && s["rank"].IsInt() ? s["rank"].GetInt() : 0);
        result.push_back(std::move(rs));
    }
    return result;
}

static ReplayData parse_replay_response(const cpr::Response& response) {
    ReplayData result;
    if (response.status_code != 200) {
        return result;
    }

    rapidjson::Document doc;
    if (doc.Parse(response.text.c_str()).HasParseError() || !doc.IsObject()) {
        return result;
    }
    if (!doc.HasMember("hash") || !doc["hash"].IsString()) {
        return result;
    }
    if (!doc.HasMember("difficulty") || !doc["difficulty"].IsInt()) {
        return result;
    }
    if (!doc.HasMember("input_log") || !doc["input_log"].IsObject()) {
        return result;
    }

    result.hash = doc["hash"].GetString();
    result.difficulty = doc["difficulty"].GetInt();
    for (auto& m : doc["input_log"].GetObject()) {
        if (!m.value.IsInt()) {
            continue;
        }

        rapidjson::Document timestamp;
        timestamp.Parse<rapidjson::kParseFullPrecisionFlag>(m.name.GetString(), m.name.GetStringLength());

        if (timestamp.HasParseError() || !timestamp.IsNumber()) {
            spdlog::warn("Replay contains an invalid timestamp");
            return {};
        }

        result.input_log.emplace(timestamp.GetDouble(), m.value.GetInt());
    }

    if (doc.HasMember("username") && doc["username"].IsString()) {
        result.player_data.username = doc["username"].GetString();
    }
    if (doc.HasMember("title") && doc["title"].IsString()) {
        result.player_data.title = doc["title"].GetString();
    }
    if (doc.HasMember("title_bg") && doc["title_bg"].IsInt()) {
        result.player_data.title_bg = doc["title_bg"].GetInt();
    }
    try {
        if (doc.HasMember("chara_color_1") && doc["chara_color_1"].IsString()) {
            result.player_data.chara_color_1 = parse_hex_color(doc["chara_color_1"].GetString());
        }
        if (doc.HasMember("chara_color_2") && doc["chara_color_2"].IsString()) {
            result.player_data.chara_color_2 = parse_hex_color(doc["chara_color_2"].GetString());
        }
        if (doc.HasMember("chara_color_3") && doc["chara_color_3"].IsString()) {
            result.player_data.chara_color_3 = parse_hex_color(doc["chara_color_3"].GetString());
        }
    } catch (const std::invalid_argument&) {
    }
    if (doc.HasMember("chara_head_index") && doc["chara_head_index"].IsInt()) {
        result.player_data.chara_head_index = doc["chara_head_index"].GetInt();
    }
    if (doc.HasMember("chara_body_index") && doc["chara_body_index"].IsInt()) {
        result.player_data.chara_body_index = doc["chara_body_index"].GetInt();
    }
    if (doc.HasMember("chara_cos_index") && doc["chara_cos_index"].IsInt()) {
        result.player_data.chara_cos_index = doc["chara_cos_index"].GetInt();
    }
    if (doc.HasMember("chara_is_costume") && doc["chara_is_costume"].IsBool()) {
        result.player_data.chara_is_costume = doc["chara_is_costume"].GetBool();
    }

    result.ok = true;
    return result;
}

void NetworkClient::request_replay(int score_id) {
    if (!network_enabled()) {
        return;
    }
    if (pending_replay_fetch.has_value()) {
        return;
    }
    std::string score_id_str = std::to_string(score_id);
    pending_replay_fetch =
        cpr::GetAsync(cpr::Url{network_url("/replay")}, signed_headers("GET", "/replay", {{"score_id", score_id_str}}),
                      cpr::Parameters{{"score_id", score_id_str}}, cpr::Timeout{10000} NETWORK_CA_OPT);
}

std::optional<ReplayData> NetworkClient::take_replay_result() {
    if (!replay_fetch_result.has_value()) {
        return std::nullopt;
    }
    std::optional<ReplayData> result = std::move(replay_fetch_result);
    replay_fetch_result.reset();
    return result;
}

void NetworkClient::clear_import_flag(const std::string& access_code) {
    if (!network_enabled()) {
        return;
    }
    cpr::Response response =
        cpr::Post(cpr::Url{network_url("/clear_import_flag")},
                  signed_headers("POST", "/clear_import_flag", {{"access_code", access_code}}),
                  cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        spdlog::error("Failed to clear import flag: HTTP {} - {}", response.status_code, response.text);
    }
}

std::string NetworkClient::register_user(const std::string& username, const std::string& idm) {
    if (!network_enabled()) {
        return "";
    }
    cpr::Payload payload{{"username", username}};
    if (!idm.empty()) {
        payload.Add({"idm", idm});
    }
    cpr::Response response = cpr::Post(cpr::Url{network_url("/register_user")},
                                       signed_headers("POST", "/register_user", {{"username", username}, {"idm", idm}}),
                                       payload, cpr::Timeout{5000} NETWORK_CA_OPT);
    if (response.status_code != 200) {
        spdlog::error("Failed to register user: HTTP {} - {}", response.status_code, response.text);
        return "";
    }
    return response.text;
}

static std::string map_to_json_impl(const std::map<double, InputLogType>& my_map) {
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    for (const auto& pair : my_map) {
        char keybuf[32];
        std::snprintf(keybuf, sizeof(keybuf), "%.17g", pair.first);
        rapidjson::Value key(keybuf, allocator);
        doc.AddMember(key, (int)pair.second, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return buffer.GetString();
}

std::string NetworkClient::map_to_json(const std::map<double, InputLogType>& my_map) {
    return map_to_json_impl(my_map);
}

void NetworkClient::submit_score(const std::string& hash, int difficulty, const std::string& access_code,
                                 const Score& score, const std::map<double, InputLogType>& input_log, int64_t played_at,
                                 const std::string& modifiers_json, bool chara_is_costume, int chara_cos_index) {
    if (!network_enabled()) {
        return;
    }
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
        {"input_log", map_to_json_impl(input_log)},
        {"played_at", played_at > 0 ? std::to_string(played_at) : ""},
        {"modifiers", modifiers_json},
        {"chara_is_costume", chara_is_costume ? "true" : "false"},
        {"chara_cos_index", std::to_string(chara_cos_index)},
    };
    if (pending_score_submit.has_value()) {
        cpr::Response response = pending_score_submit->get();
        pending_score_submit.reset();
        if (response.status_code != 200) {
            spdlog::error("Failed to submit score: HTTP {} - {}", response.status_code, response.text);
        }
    }
    pending_score_submit =
        cpr::PostAsync(cpr::Url{network_url("/submit_score")}, signed_headers("POST", "/submit_score", params),
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
                           {"input_log", params["input_log"]},
                           {"played_at", params["played_at"]},
                           {"modifiers", params["modifiers"]},
                           {"chara_is_costume", params["chara_is_costume"]},
                           {"chara_cos_index", params["chara_cos_index"]},
                       },
                       cpr::Timeout{5000} NETWORK_CA_OPT);
}

void NetworkClient::poll_song_jump(const std::string& access_code) {
    if (!network_enabled()) {
        return;
    }
    if (pending_song_jump.has_value()) {
        return;
    }
    pending_song_jump = cpr::GetAsync(cpr::Url{network_url("/poll_song_jump")},
                                      signed_headers("GET", "/poll_song_jump", {{"access_code", access_code}}),
                                      cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
}

std::optional<std::string> NetworkClient::take_song_jump_result() {
    if (!song_jump_result.has_value()) {
        return std::nullopt;
    }
    std::optional<std::string> result = std::move(song_jump_result);
    song_jump_result.reset();
    return result;
}

void NetworkClient::poll_replay_jump(const std::string& access_code) {
    if (!network_enabled()) {
        return;
    }
    if (pending_replay_jump.has_value()) {
        return;
    }
    pending_replay_jump =
        cpr::GetAsync(cpr::Url{network_url("/poll_replay_jump")},
                      signed_headers("GET", "/poll_replay_jump", {{"access_code", access_code}}),
                      cpr::Parameters{{"access_code", access_code}}, cpr::Timeout{5000} NETWORK_CA_OPT);
}

std::optional<int> NetworkClient::take_replay_jump_result() {
    if (!replay_jump_result.has_value()) {
        return std::nullopt;
    }
    std::optional<int> result = replay_jump_result;
    replay_jump_result.reset();
    return result;
}

void NetworkClient::update(double current_ms) {
#if defined(__ANDROID__)
    if (pending_update_checksum.has_value() &&
        pending_update_checksum->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_update_checksum->get();
        pending_update_checksum.reset();

        if (response.status_code != 200) {
            spdlog::warn("Update check: could not fetch checksums-android.sha256 (HTTP {})", response.status_code);
        } else {
            std::string expected_sha256;
            std::istringstream(response.text) >> expected_sha256;

            std::string installed_sha256;
            if (std::ifstream marker(kUpdateMarkerPath); marker) {
                std::getline(marker, installed_sha256);
            }

            if (installed_sha256 == expected_sha256) {
                spdlog::info("Update check: APK up to date");
            } else {
                spdlog::info("Update check: newer APK available, downloading");
                pending_update_expected_sha256 = expected_sha256;
                std::error_code mkdir_ec;
                fs::create_directories(fs::path(kUpdateApkTmpPath).parent_path(), mkdir_ec);
                pending_update_apk = cpr::DownloadAsync(fs::path(kUpdateApkTmpPath), cpr::Url{kUpdateApkUrl},
                                                        cpr::Timeout{600000}, cpr::ConnectTimeout{5000} NETWORK_CA_OPT);
            }
        }
    }

    if (pending_update_apk.has_value() &&
        pending_update_apk->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_update_apk->get();
        pending_update_apk.reset();

        std::optional<std::string> actual_sha256 =
            response.status_code == 200 ? sha256_of_file(kUpdateApkTmpPath) : std::nullopt;

        if (response.status_code != 200) {
            spdlog::error("Update: APK download failed (HTTP {})", response.status_code);
            std::error_code ec;
            fs::remove(kUpdateApkTmpPath, ec);
        } else if (!actual_sha256 || *actual_sha256 != pending_update_expected_sha256) {
            spdlog::error("Update: downloaded APK sha256 mismatch, discarding");
            std::error_code ec;
            fs::remove(kUpdateApkTmpPath, ec);
        } else {
            std::error_code ec;
            fs::rename(kUpdateApkTmpPath, kUpdateApkPath, ec);
            if (ec) {
                spdlog::error("Update: failed to move {} to {}: {}", kUpdateApkTmpPath, kUpdateApkPath, ec.message());
            } else {
                std::ofstream(kUpdateMarkerPath, std::ios::trunc) << pending_update_expected_sha256;
                install_apk(kUpdateApkPath);
            }
        }
    }
#endif

    if (!network_enabled()) {
        online = false;
        probing_ = false;
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
        if (probing_ && !online) {
            spdlog::warn("Network: server unreachable (HTTP {}), skipping profile sync", response.status_code);
        }
        probing_ = false;
        if (online != was_online) {
            spdlog::info("hiroba heartbeat: {}", online ? "online" : "offline");
        }

        if (online) {
            rapidjson::Document doc;
            if (!doc.Parse(response.text.c_str()).HasParseError() && doc.HasMember("min_client_version") &&
                doc["min_client_version"].IsString()) {
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
            if (!doc.Parse(response.text.c_str()).HasParseError() && doc.HasMember("hash") && doc["hash"].IsString()) {
                song_jump_result = doc["hash"].GetString();
            }
        }
    }

    if (pending_replay_jump.has_value() &&
        pending_replay_jump->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_replay_jump->get();
        pending_replay_jump.reset();

        if (response.status_code == 200) {
            rapidjson::Document doc;
            if (!doc.Parse(response.text.c_str()).HasParseError() && doc.HasMember("score_id") &&
                doc["score_id"].IsInt()) {
                replay_jump_result = doc["score_id"].GetInt();
            }
        }
    }

    if (pending_replay_fetch.has_value() &&
        pending_replay_fetch->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_replay_fetch->get();
        pending_replay_fetch.reset();
        replay_fetch_result = parse_replay_response(response);
    }

    if (pending_score_submit.has_value() &&
        pending_score_submit->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        cpr::Response response = pending_score_submit->get();
        pending_score_submit.reset();
        if (response.status_code != 200) {
            spdlog::error("Failed to submit score: HTTP {} - {}", response.status_code, response.text);
        }
    }
}

void NetworkClient::shutdown() {
    if (pending_heartbeat.has_value()) {
        pending_heartbeat->wait();
        pending_heartbeat.reset();
    }
    if (pending_song_jump.has_value()) {
        pending_song_jump->wait();
        pending_song_jump.reset();
    }
    if (pending_replay_jump.has_value()) {
        pending_replay_jump->wait();
        pending_replay_jump.reset();
    }
    if (pending_replay_fetch.has_value()) {
        pending_replay_fetch->wait();
        pending_replay_fetch.reset();
    }
#if defined(__ANDROID__)
    if (pending_update_checksum.has_value()) {
        pending_update_checksum->wait();
        pending_update_checksum.reset();
    }
    if (pending_update_apk.has_value()) {
        pending_update_apk->wait();
        pending_update_apk.reset();
    }
#endif
    if (pending_score_submit.has_value()) {
        cpr::Response response = pending_score_submit->get();
        pending_score_submit.reset();
        if (response.status_code != 200) {
            spdlog::error("Failed to submit score: HTTP {} - {}", response.status_code, response.text);
        }
    }
}

#else

bool NetworkClient::probe_online() {
    return false;
}
void NetworkClient::start_probe() {}
std::string NetworkClient::register_user(const std::string&, const std::string&) {
    return "";
}
void NetworkClient::submit_score(const std::string&, int, const std::string&, const Score&,
                                 const std::map<double, InputLogType>&, int64_t, const std::string&, bool, int) {}
bool NetworkClient::check_import_requested(const std::string&) {
    return false;
}
void NetworkClient::clear_import_flag(const std::string&) {}
bool NetworkClient::fetch_chara_colors(const std::string&, Color&, Color&, Color&) {
    return false;
}
bool NetworkClient::fetch_username(const std::string&, std::string&) {
    return false;
}
void NetworkClient::update_username(const std::string&, const std::string&) {}
bool NetworkClient::fetch_title(const std::string&, std::string&) {
    return false;
}
bool NetworkClient::fetch_title_bg(const std::string&, int&) {
    return false;
}
bool NetworkClient::fetch_costume(const std::string&, int&, int&, int&, bool&) {
    return false;
}
std::optional<RemoteUser> NetworkClient::fetch_user(const std::string&) {
    return std::nullopt;
}
void NetworkClient::check_and_install_android_update() {}
void NetworkClient::update_costume(const std::string&, int, int, int, bool) {}
std::vector<RemoteScore> NetworkClient::fetch_scores(const std::string&) {
    return {};
}
void NetworkClient::poll_song_jump(const std::string&) {}
std::optional<std::string> NetworkClient::take_song_jump_result() {
    return std::nullopt;
}
void NetworkClient::poll_replay_jump(const std::string&) {}
std::optional<int> NetworkClient::take_replay_jump_result() {
    return std::nullopt;
}
void NetworkClient::request_replay(int) {}
std::optional<ReplayData> NetworkClient::take_replay_result() {
    return std::nullopt;
}
void NetworkClient::update(double) {}
void NetworkClient::shutdown() {}

#endif
