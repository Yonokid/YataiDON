#pragma once

#include <array>
#include <optional>

#include "../../../libs/file/scores.h"
#include "../../../libs/file/script.h"

class ScoreHistory : public LuaScript {
public:
    ScoreHistory(const std::array<std::optional<Score>, 5>& scores, const std::array<std::string, 5>& hashes,
                 double current_ms);

    void update(double current_ms);
    void draw(double open_ms);

private:
    sol::protected_function fn_update, fn_draw, fn_set_ranking;
    std::string rank_hash;
    int rank_diff = 0;
};
