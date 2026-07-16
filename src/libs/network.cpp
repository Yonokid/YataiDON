#include "network.h"
#include "scores.h"
#include <spdlog/spdlog.h>

NetworkClient network;

#if defined(NETWORK_ENABLED)

void NetworkClient::check_heartbeat() {
    if (pending_heartbeat.has_value()) return;
    pending_heartbeat = cpr::GetAsync(
        cpr::Url{std::string(NETWORK_URL) + "/health"},
        cpr::Header{{"Authorization", "Bearer " NETWORK_AUTH_KEY}},
        cpr::Timeout{2000}
    );
}

std::string NetworkClient::register_user(const std::string& username) {
    cpr::Response response = cpr::Post(
        cpr::Url{std::string(NETWORK_URL) + "/register_user"},
        cpr::Header{{"Authorization", "Bearer " NETWORK_AUTH_KEY}},
        cpr::Payload{{"username", username}},
        cpr::Timeout{5000}
    );
    if (response.status_code != 200) {
        spdlog::error("Failed to register user: HTTP {} - {}", response.status_code, response.text);
        return "";
    }
    return response.text;
}

void NetworkClient::submit_score(std::string& hash, int difficulty, const std::string& access_code, Score score) {
    cpr::Response response = cpr::Post(
        cpr::Url{std::string(NETWORK_URL) + "/submit_score"},
        cpr::Header{{"Authorization", "Bearer " NETWORK_AUTH_KEY}},
        cpr::Parameters{
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
        },
        cpr::Timeout{5000}
    );
    if (response.status_code != 200) {
        spdlog::error("Failed to submit score: HTTP {} - {}", response.status_code, response.text);
    }
}

void NetworkClient::update(double current_ms) {
    if (current_ms - last_heartbeat_ms >= HEARTBEAT_INTERVAL_MS) {
        last_heartbeat_ms = current_ms;
        check_heartbeat();
    }

    if (!pending_heartbeat.has_value()) return;
    if (pending_heartbeat->wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

    cpr::Response response = pending_heartbeat->get();
    pending_heartbeat.reset();

    bool was_online = online;
    online = response.status_code == 200;
    if (online != was_online) {
        spdlog::info("hiroba heartbeat: {}", online ? "online" : "offline");
    }
}

#else

std::string NetworkClient::register_user(const std::string&) { return ""; }
void NetworkClient::submit_score(std::string&, int, const std::string&, Score) {}
void NetworkClient::update(double) {}

#endif
