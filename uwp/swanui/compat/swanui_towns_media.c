/* Tsugaru UWP port (FM Towns) - CD / floppy media of a game file (see swanui_towns_media.h).
 * Compiled into the frontend library with compat/swanui_compat.h force-included, so fopen/stat/opendir below are
 * the brokered UTF-8 versions.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "swanui_towns_media.h"

#include "SwanStationPS5.h" /* SwanStationPS5_PATH_MAX, path_join, path_ext, str_icmp, str_copy */

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define FLOPPY_MAX_BYTES (2 * 1024 * 1024) /* the biggest FM Towns floppy is 1.44 MB */

static bool ext_is(const char *name, const char *ext)
{
    return str_icmp(path_ext(name), ext) == 0;
}

static bool is_raw_ext(const char *name)
{
    return ext_is(name, "bin") || ext_is(name, "img");
}

static int media_of_file(const char *path, int depth)
{
    if (ext_is(path, "d77") || ext_is(path, "d88"))
        return TOWNS_MEDIA_FD;
    if (is_raw_ext(path))
    {
        struct stat st;
        if (stat(path, &st) == 0 && st.st_size > 0 && st.st_size <= FLOPPY_MAX_BYTES)
            return TOWNS_MEDIA_FD;
        return TOWNS_MEDIA_CD;
    }
    if (ext_is(path, "m3u") && depth < 2)
    {
        FILE *f = fopen(path, "r");
        if (f)
        {
            char line[SwanStationPS5_PATH_MAX];
            while (fgets(line, sizeof(line), f))
            {
                line[strcspn(line, "\r\n")] = '\0';
                if (!line[0] || line[0] == '#')
                    continue;
                char dir[SwanStationPS5_PATH_MAX], full[SwanStationPS5_PATH_MAX];
                str_copy(dir, sizeof(dir), path);
                char *slash = strrchr(dir, '/');
                if (slash)
                    *slash = '\0';
                path_join(full, sizeof(full), dir, line);
                fclose(f);
                return media_of_file(full, depth + 1);
            }
            fclose(f);
        }
    }
    return TOWNS_MEDIA_CD;
}

int towns_media_of_path(const char *path)
{
    return (path && path[0]) ? media_of_file(path, 0) : TOWNS_MEDIA_CD;
}

static void stem_of(const char *name, char *out, size_t size)
{
    str_copy(out, size, name);
    char *dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

static bool starts_icase(const char *s, const char *prefix)
{
    for (; *prefix; ++s, ++prefix)
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix))
            return false;
    return true;
}

int towns_companion_fd(const char *game_path, char *out, size_t size)
{
    if (size)
        out[0] = '\0';
    if (!game_path || !game_path[0] || towns_media_of_path(game_path) == TOWNS_MEDIA_FD)
        return 0;

    char dir[SwanStationPS5_PATH_MAX], game_stem[256];
    str_copy(dir, sizeof(dir), game_path);
    char *slash = strrchr(dir, '/');
    if (!slash)
        return 0;
    *slash = '\0';
    stem_of(slash + 1, game_stem, sizeof(game_stem));

    DIR *d = opendir(dir);
    if (!d)
        return 0;
    char best_prefix[256] = "", best_any[256] = "";
    int cd_images = 0; /* sheets/ISOs in the folder: one means a folder that holds a single game */
    struct dirent *e;
    while ((e = readdir(d)))
    {
        const char *name = e->d_name;
        if (name[0] == '.')
            continue;
        char full[SwanStationPS5_PATH_MAX];
        path_join(full, sizeof(full), dir, name);
        if (ext_is(name, "cue") || ext_is(name, "iso") || ext_is(name, "mds"))
        {
            ++cd_images;
            continue;
        }
        if (!(ext_is(name, "d77") || ext_is(name, "d88") || is_raw_ext(name)))
            continue;
        if (media_of_file(full, 1) != TOWNS_MEDIA_FD)
            continue;
        if (starts_icase(name, game_stem) && (!best_prefix[0] || str_icmp(name, best_prefix) < 0))
            str_copy(best_prefix, sizeof(best_prefix), name);
        if (!best_any[0] || str_icmp(name, best_any) < 0)
            str_copy(best_any, sizeof(best_any), name);
    }
    closedir(d);

    const char *pick = best_prefix[0] ? best_prefix : (cd_images <= 1 ? best_any : "");
    if (!pick[0])
        return 0;
    path_join(out, size, dir, pick);
    return 1;
}

void towns_serial_from_title(const char *title, char *out, size_t size)
{
    uint32_t h = 2166136261u; /* FNV-1a over the letters and digits, case folded: renaming punctuation keeps the id */
    for (const unsigned char *p = (const unsigned char *)(title ? title : ""); *p; ++p)
        if (isalnum(*p) || *p >= 0x80)
        {
            h ^= (uint32_t)tolower(*p);
            h *= 16777619u;
        }
    snprintf(out, size, "FMT-%08X", (unsigned)h);
}
