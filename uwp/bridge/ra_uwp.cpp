// Tsugaru UWP port - RetroAchievements as safe no-ops: Tsugaru has no rcheevos hooks and RetroAchievements has no
// FM Towns set. Every ra_* the frontend links answers "not available": signed out, no game progress, no messages.
// ra_password_login (src/ra/login.h) is not compiled from the frontend, so it is defined here too.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstring>

#include "swanui_cdecl.h"
#define Settings SwanSettings
extern "C" {
#include "src/SwanStationPS5.h"
#include "src/ra/achievements.h"
#include "src/ra/login.h"
}
#undef Settings

namespace {
bool s_hardcore = false; // kept only so the settings switch does not bounce back

void CopyString(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0)
        return;
    std::strncpy(dst, src ? src : "", size - 1);
    dst[size - 1] = '\0';
}
} // namespace

extern "C" {

void ra_init(const Paths *paths)
{
    (void)paths;
    SwanStationPS5_log("ra: not available (no RetroAchievements for the FM Towns)");
}

void ra_shutdown(void)
{
}

void ra_game_loaded(const char *game_path)
{
    (void)game_path;
}

void ra_game_unloaded(void)
{
}

void ra_frame(void)
{
}

void ra_idle(void)
{
}

void ra_reset(void)
{
}

bool ra_signed_in(void)
{
    return false;
}

const char *ra_user(void)
{
    return "";
}

unsigned ra_user_score(void)
{
    return 0;
}

bool ra_game_progress(int *unlocked, int *total)
{
    if (unlocked)
        *unlocked = 0;
    if (total)
        *total = 0;
    return false;
}

int ra_list(RaAchievement *out, int max)
{
    (void)out;
    (void)max;
    return 0;
}

int ra_recent(RaRecent *out, int max)
{
    (void)out;
    (void)max;
    return 0;
}

unsigned ra_user_hardcore_score(void)
{
    return 0;
}

unsigned ra_user_softcore_score(void)
{
    return 0;
}

void ra_use_token(const char *user, const char *token)
{
    (void)user;
    (void)token;
}

void ra_sign_out(void)
{
}

bool ra_hardcore(void)
{
    return false; // never restricts save states, cheats or rewind: nothing is signed in
}

void ra_set_hardcore(bool on)
{
    s_hardcore = on;
}

bool ra_warning(bool *emulator, bool *version)
{
    if (emulator)
        *emulator = false;
    if (version)
        *version = false;
    return false;
}

void ra_game_summary(char *out, size_t size)
{
    CopyString(out, size, "");
}

bool ra_next_message(char *title, size_t title_size, char *detail, size_t detail_size, bool *quiet)
{
    CopyString(title, title_size, "");
    CopyString(detail, detail_size, "");
    if (quiet)
        *quiet = false;
    return false;
}

bool ra_tracker(char *title, size_t title_size, char *progress, size_t progress_size, float *percent)
{
    CopyString(title, title_size, "");
    CopyString(progress, progress_size, "");
    if (percent)
        *percent = 0.0f;
    return false;
}

bool ra_password_login(const char *name, const char *password, char *token, size_t token_size, char *who,
                       size_t who_size, char *error, size_t error_size)
{
    (void)name;
    (void)password;
    CopyString(token, token_size, "");
    CopyString(who, who_size, "");
    CopyString(error, error_size, "RetroAchievements is not available for the FM Towns");
    return false;
}

} // extern "C"
