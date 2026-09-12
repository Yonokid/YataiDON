#pragma once

// iOS-only platform services. Paths are resolved anew on every launch because
// installing an update can relocate both the bundle and the data container.
void ios_prepare_filesystem();
double ios_game_time_ms();
void ios_set_suspended(bool suspended);
bool ios_is_suspended();
