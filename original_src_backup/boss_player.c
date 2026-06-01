Total Bytes: 7677
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
#include "boss_player.h"
#include "map.h"
#include <string.h>

extern int gAtomBombActive;

#define GRAVITY 4000.0f
#define JUMP_FORCE -1080.0f
#define HURT_INVINCIBLE_TIME 1.0f
#define CHEAT_BUFFER_SIZE 16

static char cheatBuffer[CHEAT_BUFFER_SIZE + 1] = {0};
static int cheatLen = 0;
float gCheatNotifTimer = 0.0f;
const char *gCheatNotifText = "";

static bool CheckCheat(const char *code) {
    int codeLen = (int)strlen(code);
    if (cheatLen < codeLen) return false;
    return strcmp(cheatBuffer + cheatLen - codeLen, code) == 0;
}

static void UpdateCheatBuffer(BossPlayer *p) {
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
                p->godMode = !p->godMode;
                gCheatNotifText = p->godMode ? "GOD MODE: ON" : "GOD MODE: OFF";
                gCheatNotifTimer = 3.0f;
                cheatLen = 0;
                cheatBuffer[0] = '\0';
            }
        }
        key = GetCharPressed();
    }
}

void InitBossPlayer(BossPlayer *p, Vector2 pos, float 
<truncated 5282 bytes>
 (unsigned char)(gCheatNotifTimer / 3.0f * 255);
        DrawText(gCheatNotifText, 1351 / 2 - 90, 760 / 2 + 100, 24,
            (Color){0, 255, 0, a});
    }

    Texture2D tex = idle;
    int maxFrames = 10;
    switch (p->state) {
        case PSTATE_IDLE: tex = idle; maxFrames = 10; break;
        case PSTATE_WALK: tex = walk; maxFrames = 12; break;
        case PSTATE_RUN: tex = run; maxFrames = 8; break;
        case PSTATE_JUMP: tex = jump; maxFrames = 3; break;
        case PSTATE_ATTACK: tex = attack; maxFrames = 8; break;
        case PSTATE_HURT: tex = hurt; maxFrames = 4; break;
    }

    int frameIdx = p->currentFrame % maxFrames;
    Rectangle source = {
        (float)frameIdx * (PLAYER_FRAME_W + PLAYER_SPACING),
        0,
        (float)PLAYER_FRAME_W,
        (float)PLAYER_FRAME_H
    };
    if (p->facingRight) source.width = -source.width;

    float scale = 2.0f;
    Rectangle dest = {
        p->position.x,
        p->position.y + 32.0f,
        (float)PLAYER_FRAME_W * scale,
        (float)PLAYER_FRAME_H * scale
    };
    Vector2 origin = {
        (float)PLAYER_FRAME_W * scale / 2.0f,
        (float)PLAYER_FRAME_H * scale
    };

    Color tint = WHITE;
    if (p->hurtTimer > 0 && ((int)(p->hurtTimer * 10) % 2 == 0)) {
        tint = (Color){255, 100, 100, 200};
    }

    DrawTexturePro(tex, source, dest, origin, 0.0f, tint);
}

void PlayerTakeDamage(BossPlayer *p, int damage) {
    if (p->godMode) return;
    if (p->hurtTimer > 0) return;
    p->hp -= damage;
    if (p->hp < 0) p->hp = 0;
    p->isHurt = true;
    p->isAttacking = false;
    p->hurtTimer = HURT_INVINCIBLE_TIME;
    p->currentFrame = 0;
    p->frameTimer = 0.0f;
}
