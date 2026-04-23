#pragma once
#include "../../libs/texture.h"
#include "../../libs/scores.h"
#include "../../libs/global_data.h"
#include <array>
#include <optional>
#include <vector>

class ScoreHistory {
public:
    ScoreHistory(const std::array<std::optional<Score>, 5>& scores, double current_ms);
    void update(double current_ms);
    void draw();

private:
    struct DiffScore { int diff; Score score; };
    std::vector<DiffScore> available;
    int curr_index = 0;
    double last_ms = 0.0;

    void draw_long();
    void draw_short();
};
