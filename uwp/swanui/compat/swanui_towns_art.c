/* Tsugaru UWP port - see swanui_towns_art.h. SPDX-License-Identifier: GPL-3.0-or-later */
#include "swanui_towns_art.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

int towns_art_url(const char *urls_asset, const char *name, char *out, size_t size)
{
    char path[SwanStationPS5_PATH_MAX];
    plat_asset_path(path, sizeof(path), urls_asset);
    FILE *f = fopen(path, "r");
    if (!f || !name || !out || !size)
    {
        if (f)
            fclose(f);
        return 0;
    }
    const size_t n = strlen(name);
    char line[1024];
    int found = 0;
    while (!found && fgets(line, sizeof(line), f))
    {
        size_t len = strlen(line);
        while (len && (line[len - 1] == 10 || line[len - 1] == 13))
            line[--len] = 0;
        if (len > n + 1 && line[n] == 9 && memcmp(line, name, n) == 0 && strncmp(line + n + 1, "https://", 8) == 0)
        {
            snprintf(out, size, "%s", line + n + 1);
            found = 1;
        }
    }
    fclose(f);
    return found;
}
