#include "boss_player.h"
#include "map.h"
#include <string.h>

extern int gAtomBombActive;
extern int gPreIntroSlowWalk;

#define GRAVITY 4000.0f
#define JUMP_FORCE -900.0f
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

void InitBossPlayer(BossPlayer *p, Vector2 pos, float groundY) {
    p->position = pos;
    p->velocity = (Vector2){0, 0};
    p->hurtBox = (Rectangle){pos.x - 10, pos.y - 30, 20, 30};
    p->speed = 250.0f;
    p->groundY = groundY;
    p->hp = PLAYER_MAX_HP;
    p->facingRight = true;
    p->isJumping = false;
    p->isRunning = false;
    p->isSprinting = false;
    p->isAttacking = false;
    p->isHurt = false;
    p->freezeTimer = 0.0f;
    p->hurtTimer = 0.0f;
    p->godMode = false;
    p->state = PSTATE_IDLE;
    p->currentFrame = 0;
    p->frameTimer = 0.0f;
    p->animSpeed = 0.1f;
}

static void UpdateBossPlayerInternal(BossPlayer *p, cute_tiled_map_t *map, float mapOffsetY, float fallbackGroundY, float dt) {
    UpdateCheatBuffer(p);
    if (gCheatNotifTimer > 0) gCheatNotifTimer -= dt;

    if (gAtomBombActive && p->hp > 0 && !p->godMode) {
        p->hp = 0;
    }

    if (p->hurtTimer > 0) {
        p->hurtTimer -= dt;
        if (p->hurtTimer <= 0) p->isHurt = false;
    }

    p->isRunning = false;
    p->isSprinting = false;
    float currentSpeed = p->speed;
    float animSpeed = 0.1f;

    if (p->freezeTimer > 0) p->freezeTimer -= dt;

    if (gPreIntroSlowWalk) {
        p->isSprinting = false;
        currentSpeed = 100.0f; // Walk slowly
        animSpeed = 0.15f; // Slow down walking animation
    } else {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
            currentSpeed *= 1.8f;
            p->isSprinting = true;
            animSpeed = 0.07f;
        }
    }

    // Đi-bộ-only (pre-intro / đi ra cửa): CẤM đánh chuột trái.
    if (!gPreIntroSlowWalk &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !p->isAttacking && !p->isHurt) {
        p->isAttacking = true;
        p->currentFrame = 0;
        p->frameTimer = 0.0f;
        p->freezeTimer = 0.15f;
    }

    if (p->isAttacking) animSpeed = 0.07f;

    bool moving = false;
    if (p->freezeTimer <= 0) {
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            p->position.x += currentSpeed * dt;
            p->facingRight = true;
            p->isRunning = true;
            moving = true;
        }
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            p->position.x -= currentSpeed * dt;
            p->facingRight = false;
            p->isRunning = true;
            moving = true;
        }
    }

    // Đi-bộ-only (pre-intro / đi ra cửa): CẤM nhảy.
    if (!gPreIntroSlowWalk &&
        (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) && !p->isJumping) {
        p->velocity.y = JUMP_FORCE;
        p->isJumping = true;
        if (!p->isAttacking) {
            p->currentFrame = 0;
            p->frameTimer = 0.0f;
        }
    }

    if (p->isJumping) {
        float currentY = p->position.y;
        p->velocity.y += GRAVITY * dt;
        p->position.y += p->velocity.y * dt;
        float floorY = map ? MapGetFloorYAtX(map, p->position.x, currentY, p->position.y, mapOffsetY, fallbackGroundY) : p->groundY;
        p->groundY = floorY;
        if (p->velocity.y >= 0.0f && p->position.y >= floorY) {
            p->position.y = floorY;
            p->velocity.y = 0;
            p->isJumping = false;
        }
    } else if (map) {
        float floorY = MapGetFloorYAtX(map, p->position.x, p->position.y, p->position.y + 12.0f, mapOffsetY, fallbackGroundY);
        p->groundY = floorY;
        if (p->position.y < floorY - 1.0f) {
            p->isJumping = true;
            p->velocity.y = 0.0f;
        } else {
            p->position.y = floorY;
        }
    }

    if (p->isHurt) {
        p->state = PSTATE_HURT;
        p->animSpeed = 0.1f;
    } else if (p->isAttacking) {
        p->state = PSTATE_ATTACK;
        p->animSpeed = animSpeed;
    } else if (p->isJumping) {
        p->state = PSTATE_JUMP;
        p->animSpeed = 0.1f;
    } else if (moving) {
        p->state = p->isSprinting ? PSTATE_RUN : PSTATE_WALK;
        p->animSpeed = p->isSprinting ? 0.07f : 0.1f;
    } else {
        p->state = PSTATE_IDLE;
        p->animSpeed = 0.12f;
    }

    p->frameTimer += dt;
    if (p->frameTimer >= p->animSpeed) {
        p->frameTimer = 0;
        p->currentFrame++;
    }

    if (p->isAttacking && p->currentFrame >= 8) {
        p->isAttacking = false;
    }

    p->hurtBox.x = p->position.x - 10;
    p->hurtBox.y = p->position.y - 30;
}

void UpdateBossPlayer(BossPlayer *p, float dt) {
    UpdateBossPlayerInternal(p, NULL, 0.0f, p->groundY, dt);
}

void UpdateBossPlayerOnMap(BossPlayer *p, cute_tiled_map_t *map, float mapOffsetY, float fallbackGroundY, float dt) {
    UpdateBossPlayerInternal(p, map, mapOffsetY, fallbackGroundY, dt);
}

void DrawBossPlayer(BossPlayer *p, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, Texture2D hurt) {
    if (p->godMode) {
        DrawText("GOD MODE", GetScreenWidth() - 180, 40, 20, (Color){255, 215, 0, 220});
    }
    if (gCheatNotifTimer > 0) {
        unsigned char a = (unsigned char)(gCheatNotifTimer / 3.0f * 255);
        DrawText(gCheatNotifText, GetScreenWidth() / 2 - 90, GetScreenHeight() / 2 + 100, 24,
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

    float scale = 1.2f;
    Rectangle dest = {
        p->position.x,
        p->position.y + 19.2f,
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

void BossPlayerTakeDamage(BossPlayer *p, int damage) {
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
