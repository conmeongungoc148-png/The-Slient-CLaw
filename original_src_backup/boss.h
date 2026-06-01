Total Bytes: 4554
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
#ifndef BOSS_H
#define BOSS_H

#include "raylib.h"
#include "projectile.h"
#include "orb.h"
#include "cute_tiled.h"
#include <stdbool.h>

#define BOSS_MAX_HP 120
#define BOSS_FRAME_W 224
#define BOSS_FRAME_H 240
#define BOSS_TOTAL_FRAMES 15

typedef enum {
    BOSS_PHASE_1,
    BOSS_PHASE_2,
    BOSS_PHASE_3,
    BOSS_PHASE_4       // Phase cuối, HP rất thấp
} BossPhase;

typedef enum {
    BOSS_INTRO,         // Boss đang xuất hiện từ dưới lên
    BOSS_ROAR,          // Boss hét + screen shake
    BOSS_FIGHTING,      // Đang chiến đấu
    BOSS_DYING,         // HP=0, đứng yên chờ nhạc ending phát hết
    BOSS_DEFEATED       // Sau khi ending hết: explosion + fade
} BossState;

typedef enum {
    ATTACK_PROJECTILE,  // Bắn projectile thường (P1+)
    ATTACK_LASER,       // Bắn laser beam (P2+)
    ATTACK_SLAM,        // Đập tay xuống tạo shockwave (P2+)
    ATTACK_HAZARD,      // Spawn hazard obstacles (P3+)
    ATTACK_CLAW,        // Vuốt cào 1 khu vực (P3+)
    ATTACK_BARRAGE,     // P4 ONLY: 360° radial burst 12 projectiles
    ATTACK_RAIN         // P4 ONLY: Rain 6 projectiles từ trên trời
} AttackType;

typedef enum {
    CLAW_ZONE_LEFT,     // Khu vực bên trái
    CLAW_ZONE_MIDDLE,   // Khu vực giữa
    CLAW_ZONE_RIGHT     // Khu vực bên phải
} ClawZone;

typedef struct {
    Vector2 pos
<truncated 1547 bytes>
ingTime;  // Thời gian warning trước khi đập
    float slamTimer;        // Thời gian shockwave (sau warning)
    Vector2 slamPos;
    float shockwaveRadius;
    
    // Hazard attack
    int hazardCount;
    Vector2 hazardPositions[5];
    float hazardWarningTime[5];
    bool hazardActive[5];
    
    // Claw attack (Phase 2+)
    bool clawActive;
    ClawZone clawZone;
    float clawWarningTime;
    float clawDuration;

    // Animation
    int currentFrame;
    float frameTimer;
    float animSpeed;

    // Visual effects
    float shakeTimer;
    float shakeIntensity;
    float scale;            // Scale để boss full screen
    
    // Death effect
    float deathTimer;       // Timer cho death animation
    float deathFlashTimer;  // Flash trắng

    // Atom bomb timer (force end nếu player kéo dài game quá lâu)
    float fightTimer;       // Đếm thời gian từ khi vào FIGHTING
    float atomTimer;        // Animation atom bomb sau khi trigger (visual)
    bool atomTriggered;     // Đã trigger atom chưa
} Boss;

void InitBoss(Boss *boss, Vector2 startPos, Vector2 targetPos);
void BossSetArenaMap(cute_tiled_map_t *map, float offsetY, float fallbackGroundY);
void UpdateBoss(Boss *boss, Vector2 playerPos, ProjectileManager *pm, OrbManager *om, float dt, float *cameraShake);
void DrawBoss(Boss *boss, Texture2D spriteSheet, float cameraOffsetY);
void BossTakeDamage(Boss *boss, int damage);
bool CheckPlayerInShockwave(Vector2 playerPos, Vector2 slamPos, float radius);
bool CheckPlayerInLaser(Vector2 playerPos, Vector2 laserStart, Vector2 laserEnd, float width);
bool CheckPlayerInClawZone(Vector2 playerPos, ClawZone zone);

#endif // BOSS_H
