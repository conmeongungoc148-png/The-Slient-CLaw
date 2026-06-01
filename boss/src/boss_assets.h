#ifndef BOSS_ASSETS_H
#define BOSS_ASSETS_H

#include "raylib.h"
#include <stdio.h>

static inline const char* GetBossAssetPath(const char* path) {
    static char buf[256];
    if (FileExists(path)) {
        return path;
    }
    snprintf(buf, sizeof(buf), "boss/%s", path);
    return buf;
}

#endif // BOSS_ASSETS_H
