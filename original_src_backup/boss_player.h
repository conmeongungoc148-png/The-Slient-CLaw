Total Bytes: 1205
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
#ifndef BOSS_PLAYER_H
#define BOSS_PLAYER_H

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
} PlayerState;

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
    PlayerState state;
    int currentFrame;
    float frameTimer;
    float animSpeed;
} BossPlayer;

void InitBossPlayer(BossPlayer *p, Vector2 pos, float groundY);
void UpdateBossPlayer(BossPlayer *p, float dt);
void UpdateBossPlayerOnMap(BossPlayer *p, cute_tiled_map_t *map, float mapOffsetY, float fallbackGroundY, float dt);
void DrawBossPlayer(BossPlayer *p, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, Texture2D hurt);
void PlayerTakeDamage(BossPlayer *p, int damage);

#endif // BOSS_PLAYER_H
