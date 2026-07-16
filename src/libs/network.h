#pragma once

#include "scores.h"

#if defined(NETWORK_URL) && defined(NETWORK_AUTH_KEY)
#define NETWORK_ENABLED 1
#include <cpr/cpr.h>
#endif

#include <string>

class NetworkClient {
public:
    void update(double current_ms);

    bool is_online() const { return online; }

    std::string register_user(const std::string& username);
    void submit_score(std::string& hash, int difficulty, const std::string& access_code, Score score);

private:
    void check_heartbeat();

    bool online = false;
#if defined(NETWORK_ENABLED)
    std::optional<cpr::AsyncResponse> pending_heartbeat;
    static constexpr double HEARTBEAT_INTERVAL_MS = 5000.0;
    double last_heartbeat_ms = -HEARTBEAT_INTERVAL_MS;
#endif
};

extern NetworkClient network;
