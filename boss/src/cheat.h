#ifndef CHEAT_H
#define CHEAT_H

#include "raylib.h"
#include "player.h"
#include <string.h>

// === GTA-style cheat code ===
// Gõ "NINELIVES" trong gameplay để toggle dev mode (mèo bất tử, 9 mạng)
#define CHEAT_BUFFER_SIZE 16

static char cheatBuffer[CHEAT_BUFFER_SIZE + 1] = {0};
static int cheatLen = 0;
static float cheatNotifTimer = 0.0f;
static const char *cheatNotifText = "";

static inline bool CheckCheat(const char *code) {
    int codeLen = (int)strlen(code);
    if (cheatLen < codeLen) return false;
    return strcmp(cheatBuffer + cheatLen - codeLen, code) == 0;
}

static inline void UpdateCheatBuffer(BossPlayer *player) {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 125) {
            char c = (char)key;
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            if (cheatLen >= CHEAT_BUFFER_SIZE) {
                memmove(cheatBuffer, cheatBuffer + 1, CHEAT_BUFFER_SIZE - 1);
                cheatLen--;
            }
            cheatBuffer[cheatLen++] = c;
            cheatBuffer[cheatLen] = '\0';

            if (CheckCheat("NINELIVES")) {
                player->godMode = !player->godMode;
                cheatNotifText = player->godMode ? "GOD MODE: ON" : "GOD MODE: OFF";
                cheatNotifTimer = 3.0f;
                cheatLen = 0;
                cheatBuffer[0] = '\0';
            }
        }
        key = GetCharPressed();
    }
}

#endif