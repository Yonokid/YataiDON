#include "global_data.h"

GlobalData global_data;

void reset_session() {
    global_data.session_data[1] = SessionData();
    global_data.session_data[2] = SessionData();
}
