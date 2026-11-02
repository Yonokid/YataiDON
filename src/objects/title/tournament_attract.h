#pragma once

#include "../../libs/file/script.h"

class TournamentAttract : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_finished;
    sol::protected_function fn_can_spin;
    sol::protected_function fn_spin;
    sol::protected_function fn_close;

public:
    explicit TournamentAttract(double current_ms);
    ~TournamentAttract() override;
    TournamentAttract(const TournamentAttract&) = delete;
    TournamentAttract& operator=(const TournamentAttract&) = delete;
    void update(double current_ms);
    void draw();
    bool is_finished();
    bool can_spin();
    void spin(int puchichara);
};
