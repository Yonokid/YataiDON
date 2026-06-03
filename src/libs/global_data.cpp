#include "global_data.h"

GlobalData global_data;

void reset_session() {
    global_data.session_data[1] = SessionData();
    global_data.session_data[2] = SessionData();
}

int get_player_id(PlayerNum player_num) {
    return (player_num == global_data.first_login_player)
        ? global_data.config->general.player_1_id
        : global_data.config->general.player_2_id;
}
