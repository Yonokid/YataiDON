#pragma once

#include "result.h"

class Result2PScreen : public ResultScreen {
private:
    std::optional<ResultPlayer> player_2;

protected:
    double reveal_end_ms() override;

public:
    Result2PScreen() : ResultScreen() {}

    void on_screen_start() override;
    std::optional<Screens> update() override;
    void draw() override;
};
