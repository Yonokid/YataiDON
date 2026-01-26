#include "global_data.h"

// Global instance
GlobalData global_data;

void reset_session() {
    global_data.session_data[1] = SessionData();
    global_data.session_data[2] = SessionData();
}
