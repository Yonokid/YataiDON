#pragma once

#include "file/scores.h"
#include "judge.h"

class NetworkClient;

struct ScoreSubmissionRequest {
    std::string player_id;
    std::string hash;
    int difficulty = 0;
    Score score{};
    const Modifiers& modifiers;
    const InputLog& input_log;
    JudgeParams judge_params;
};

struct RemoteScoreSubmission {
    std::string access_code;
    std::string collection;
    int gauge = -1;
};

Score completed_play_score(Score score, bool skipped, bool cleared);

std::optional<RemoteScoreSubmission> score_submission_for(const std::string& player_id, const ScoresManager& scores,
                                                          const Config& config, const std::string& collection = "",
                                                          int gauge = -1);

class ScoreSubmitter {
public:
    ScoreSubmitter(ScoresManager& scores, NetworkClient& network) : scores(scores), network(network) {}

    // Saves locally before optional remote submission. Returns the profile when submission is called, before
    // acknowledgement.
    std::optional<PlayerData> submit(const ScoreSubmissionRequest& request,
                                     const std::optional<RemoteScoreSubmission>& submission,
                                     int64_t* score_id = nullptr);

private:
    ScoresManager& scores;
    NetworkClient& network;
};
