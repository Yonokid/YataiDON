#include "score_submission.h"

#include "network/network.h"

Score completed_play_score(Score score, bool skipped, bool cleared) {
    if (skipped) {
        score.crown = Crown::NONE;
    } else if (score.ok == 0 && score.bad == 0) {
        score.crown = Crown::DFC;
    } else if (score.bad == 0) {
        score.crown = Crown::FC;
    } else {
        score.crown = cleared ? Crown::CLEAR : Crown::NONE;
    }

    score.rank = skipped ? Rank::_NONE : rank_for_score(score.score);
    return score;
}

std::optional<RemoteScoreSubmission> score_submission_for(const std::string& player_id, const ScoresManager& scores,
                                                          const Config& config, const std::string& collection,
                                                          int gauge) {
    if (config.general.score_method == ScoreMethod::GEN3) {
        return std::nullopt;
    }

    std::string access_code;

    if (player_id == scores.player_1) {
        access_code = config.network.access_code_1;
    } else if (player_id == scores.player_2) {
        access_code = config.network.access_code_2;
    }

    if (access_code.empty()) {
        return std::nullopt;
    }

    return RemoteScoreSubmission{.access_code = std::move(access_code), .collection = collection, .gauge = gauge};
}

std::optional<PlayerData> ScoreSubmitter::submit(const ScoreSubmissionRequest& request,
                                                 const std::optional<RemoteScoreSubmission>& submission,
                                                 int64_t* score_id) {
    const int64_t played_at = unix_now();
    const std::string modifiers_json = modifiers_to_json(request.modifiers);

    scores.save_score(request.hash, request.difficulty, request.player_id, request.score, played_at, modifiers_json,
                      score_id);

    if (!submission) {
        return std::nullopt;
    }

    const PlayerData player = scores.get_player_data(request.player_id).value_or(PlayerData{});

    network.submit_score(request.hash, request.difficulty, submission->access_code, request.score, request.input_log,
                         played_at, modifiers_json, player.chara_is_costume, player.chara_cos_index,
                         submission->collection, submission->gauge, request.judge_params);

    return player;
}
