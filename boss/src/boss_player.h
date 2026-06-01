#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "cute_tiled.h"
#include <stdbool.h>

#define PLAYER_MAX_HP 10
#define PLAYER_FRAME_W 64
#define PLAYER_FRAME_H 64
#define PLAYER_SPACING 16

typedef enum {
    PSTATE_IDLE,
    PSTATE_WALK,
    PSTATE_RUN,
    PSTATE_JUMP,
    PSTATE_ATTACK,
    PSTATE_HURT
} BossPlayerState;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Rectangle hurtBox;
    float speed;
    float groundY;
    int hp;
    bool facingRight;
    bool isJumping;
    bool isRunning;
    bool isSprinting;
    bool isAttacking;
    bool isHurt;
    float freezeTimer;
    float hurtTimer;
    bool godMode;
    BossPlayerState state;
    int currentFrame;
    float frameTimer;
    float animSpeed;
} BossPlayer;

void InitBossPlayer(BossPlayer *p, Vector2 pos, float groundY);
void UpdateBossPlayer(BossPlayer *p, float dt);
void UpdateBossPlayerOnMap(BossPlayer *p, cute_tiled_map_t *map, float mapOffsetY, float fallbackGroundY, float dt);
void DrawBossPlayer(BossPlayer *p, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, Texture2D hurt);
void BossPlayerTakeDamage(BossPlayer *p, int damage);

#endif
