#include "boss.h"
#include "collision.h"
#include "map.h"
#include "boss_assets.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Lazy-loaded sprites for visual effects
static Texture2D explosionTex = {0};
static Texture2D laserTex = {0};
static bool spritesLoaded = false;

// === STATUE ANIMATION TEXTURES ===
// Grid layout: 4 cols x 4 rows, each tile 64x64 pixels, sheet 256x256
static Texture2D statueTex[3] = {0};  // 0=activating 1=active 2=shatter
#define STATUE_SHEET_COLS     4
#define STATUE_TILE_SIZE      64
// Frame counts from TSX animation definitions
#define STATUE_ACTIVATING_FRAMES  15   // tiles 0-14 (sequential)
#define STATUE_ACTIVE_FRAMES      13   // non-sequential tile IDs per TSX
#define STATUE_SHATTER_FRAMES     15   // tiles 0-10, 12-15 (skip 11)
#define STATUE_FRAME_TIME         0.10f  // 100ms per frame (matches TSX duration)

// Non-sequential tile IDs for idle animation (from statueindle.tsx)
static const int STATUE_ACTIVE_TILE_IDS[STATUE_ACTIVE_FRAMES] = {
    1, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15
};
// Non-sequential tile IDs for shatter animation (from statuedestruction.tsx, skips tile 11)
static const int STATUE_SHATTER_TILE_IDS[STATUE_SHATTER_FRAMES] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15
};

// === ATOM BOMB GLOBAL ===
// Player.c reads this flag to force-kill cat khi atom bomb trigger
int gAtomBombActive = 0;
int gPreIntroSlowWalk = 0;

#define ATOM_TAUNT_TIME 270.0f   // 4:30 - boss bắt đầu trash talk
#define ATOM_TRIGGER_TIME 300.0f // 5:00 - bom nổ
#define EXPLOSION_FRAMES 12
#define EXPLOSION_FW 96
#define EXPLOSION_FH 96
#define LASER_FRAMES 8
#define LASER_COLS 4
#define LASER_FW 300
#define LASER_FH 1309

static Texture2D splashTexs[9] = {0};

static void LoadBossSprites(void) {
    if (spritesLoaded) return;
    explosionTex = LoadTexture(GetBossAssetPath("assets/effects/explosion/Explosion.png"));
    laserTex = LoadTexture(GetBossAssetPath("assets/effects/laser/spritesheet.png"));
    for (int i = 0; i < 9; i++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/effects/vfx/splash/Frames/Vampire_skill2_frame%d.png", i + 1);
        splashTexs[i] = LoadTexture(GetBossAssetPath(path));
    }
    // Statue animation textures - correct filenames from boss/assets/statue/
    statueTex[0] = LoadTexture(GetBossAssetPath("assets/statue/statuestart.png"));
    statueTex[1] = LoadTexture(GetBossAssetPath("assets/statue/statueindle.png"));
    statueTex[2] = LoadTexture(GetBossAssetPath("assets/statue/statuedestruction.png"));
    spritesLoaded = true;
}

void UnloadBossAssets(void) {
    if (spritesLoaded) {
        UnloadTexture(explosionTex);
        UnloadTexture(laserTex);
        for (int i = 0; i < 9; i++) {
            UnloadTexture(splashTexs[i]);
            splashTexs[i] = (Texture2D){0};
        }
        for (int i = 0; i < 3; i++) {
            UnloadTexture(statueTex[i]);
            statueTex[i] = (Texture2D){0};
        }
        explosionTex = (Texture2D){0};
        laserTex = (Texture2D){0};
        spritesLoaded = false;
    }
}

#define PROJECTILE_SPEED_BASE 250.0f
#define INTRO_DURATION 15.5f
#define ROAR_DURATION 2.0f
#define LASER_CHARGE_TIME 2.0f   // 2s cảnh báo nhấp nháy
#define LASER_DURATION 2.0f      // Max duration (thường tắt sớm khi chạm mép)
#define LASER_EXTEND_SPEED 300.0f // Tốc độ laser kéo dài từ lockPos theo direction
#define LASER_MIN_PLAYER_X 200.0f // Player phải ở trong vùng này mới ra laser
#define LASER_MAX_PLAYER_X 1080.0f
#define SLAM_WARNING_TIME 2.0f   // Tăng từ 1.2 → 2.0 cho player thời gian phản ứng
#define SLAM_DURATION 1.0f
#define HAZARD_WARNING_TIME 2.5f
#define ORB_CHARGE_TIME 1.5f
#define CLAW_WARNING_TIME 0.8f
#define CLAW_DURATION 0.6f

// Vị trí tay boss (relative to boss center) - dùng để spawn orb/projectile
#define LEFT_HAND_OFFSET_X (-100.0f)
#define LEFT_HAND_OFFSET_Y (50.0f)
#define RIGHT_HAND_OFFSET_X (100.0f)
#define RIGHT_HAND_OFFSET_Y (50.0f)

static cute_tiled_map_t *arenaMap = NULL;
static float arenaMapOffsetY = 0.0f;
static float arenaFallbackGroundY = 620.0f;

static float GetSurfaceYAtX(float x) {
    float ground = arenaFallbackGroundY;
    if (arenaMap) {
        ground = MapGetFloorYAtX(arenaMap, x, -10000.0f, 10000.0f, arenaMapOffsetY, arenaFallbackGroundY);
    }
    return ground;                                           // Ground
}

void BossSetArenaMap(cute_tiled_map_t *map, float offsetY, float fallbackGroundY) {
    arenaMap = map;
    arenaMapOffsetY = offsetY;
    arenaFallbackGroundY = fallbackGroundY;
}

void InitBoss(Boss *boss, Vector2 startPos, Vector2 targetPos) {
    boss->position = startPos;  // Bắt đầu từ dưới màn hình
    boss->targetY = targetPos.y;
    boss->hp = BOSS_MAX_HP;
    boss->maxHp = BOSS_MAX_HP;
    boss->phase = BOSS_PHASE_1;
    boss->state = BOSS_PRE_INTRO;
    boss->defeated = false;
    
    // Pre-intro initialization
    boss->preIntroLightProgress = 0.0f;
    boss->preIntroTriggered = false;
    float actualGroundY = arenaMap ? (MapGetGroundY(arenaMap) + arenaMapOffsetY) : arenaFallbackGroundY;
    boss->beaconPos = (Vector2){ 608.0f, actualGroundY };

    // Intro
    boss->introTimer = 0.0f;
    boss->roarTimer = 0.0f;

    // Attack system
    boss->attackTimer = 0.0f;
    boss->attackInterval = 2.4f;
    boss->nextAttack = ATTACK_PROJECTILE;
    boss->lastAttack = ATTACK_PROJECTILE;
    for (int i = 0; i < 9; i++) boss->attackCooldowns[i] = 0.0f;

    // Trash talk
    boss->tauntTimer = 0.0f;
    boss->tauntText = "";

    // Orb system
    boss->orbTimer = 0.0f;
    boss->orbInterval = 5.0f;
    boss->orbActive = false;
    boss->orbChargeTimer = 0.0f;
    boss->orbSpawnPos = (Vector2){0, 0};

    // Statue system
    boss->mapStatueCount = 0;
    boss->statuesInitialized = false;
    for (int i = 0; i < MAX_MAP_STATUES; i++) {
        boss->mapStatues[i].animState = STATUE_INACTIVE;
        boss->mapStatues[i].animTimer = 0.0f;
        boss->mapStatues[i].currentFrame = 0;
        boss->mapStatues[i].active = false;
        boss->mapStatues[i].respawnTimer = 0.0f;
    }

    // Laser
    boss->laserActive = false;
    boss->laserChargeTime = 0.0f;
    boss->laserDuration = 0.0f;
    boss->laserStart = (Vector2){0, 0};
    boss->laserEnd = (Vector2){0, 0};
    boss->laserDirection = (Vector2){1, 0};

    // Slam
    boss->slamActive = false;
    boss->slamWarningTime = 0.0f;
    boss->slamTimer = 0.0f;
    boss->slamPos = (Vector2){0, 0};
    boss->shockwaveRadius = 0.0f;

    // Hazard
    boss->hazardCount = 0;
    for (int i = 0; i < 5; i++) {
        boss->hazardPositions[i] = (Vector2){0, 0};
        boss->hazardWarningTime[i] = 0.0f;
        boss->hazardActive[i] = false;
    }
    
    // Claw
    boss->clawActive = false;
    boss->clawZone = CLAW_ZONE_LEFT;
    boss->clawWarningTime = 0.0f;
    boss->clawDuration = 0.0f;

    // Rain
    boss->rainActive = false;
    boss->rainWarningTime = 0.0f;
    boss->rainDuration = 0.0f;

    // Animation
    boss->currentFrame = 0;
    boss->frameTimer = 0.0f;
    boss->animSpeed = 0.1f;

    // Visual
    boss->shakeTimer = 0.0f;
    boss->shakeIntensity = 0.0f;
    boss->scale = 2.7f;  // Boss to full screen (slightly smaller)
    
    // Death
    boss->deathTimer = 0.0f;
    boss->deathFlashTimer = 0.0f;

    // Atom bomb
    boss->fightTimer = 0.0f;
    boss->atomTimer = 0.0f;
    boss->atomTriggered = false;
    gAtomBombActive = 0;  // Reset flag khi init/restart

    // Fake-death twist
    boss->fakeDeathDone = false;
    boss->fakeDeathTimer = 0.0f;

    // Boom node system
    boss->boomsRemaining = 0;
    boss->boomsSpawned = false;
    boss->boomStaggerTimer = 0.0f;
    boss->boomNextOrbDelay = 4.0f;
    boss->phaseHitFlash = 0.0f;
    boss->walkAwayDoorActive = false;
    boss->reviveWalkTimer = 0.0f;

    // Skill sequencing & special laser logic
    boss->skillRoundType = ATTACK_SLAM;
    boss->skillRoundCastCount = 0;
    boss->skillRoundActive = false;
    boss->lastRoundType = ATTACK_PROJECTILE;
    boss->boomLaserSkillActive = false;
    boss->activeBoomSkillType = 0;
    boss->boomSkillTimer = 0.0f;

    // Hurtbox
    boss->hurtBox = (Rectangle){
        targetPos.x - 100, targetPos.y - 100, 200, 200
    };
}

// === TRASH TALK SYSTEM ===
static const char *TAUNTS_ATK[] = {"HET TRON DI!", "CHAM QUA!", "HAHAHA!", "MEOW~", "YEU THE?"};
static const char *TAUNTS_HIT[] = {"DAU!", "ARGH!", "DUOC LAM!", "..."};
static const char *TAUNTS_P4[] = {"MAY CHET CHAC!", "FINAL FORM!", "KHONG THE NE!", "HET DUONG CHAY!"};

static void TriggerTaunt(Boss *boss, const char **pool, int poolSize, int chance) {
    if (rand() % 100 >= chance) return;
    if (boss->tauntTimer > 0.5f) return;
    boss->tauntText = pool[rand() % poolSize];
    boss->tauntTimer = 2.5f;
}

static void UpdatePhase(Boss *boss) {
    float hpPercent = (float)boss->hp / boss->maxHp;
    BossPhase newPhase = boss->phase;

    if (hpPercent > 0.80f) newPhase = BOSS_PHASE_1;
    else if (hpPercent > 0.55f) newPhase = BOSS_PHASE_2;
    else if (hpPercent > 0.25f) newPhase = BOSS_PHASE_3;
    else newPhase = BOSS_PHASE_4;

    if (newPhase != boss->phase) {
        BossPhase oldPhase = boss->phase;
        boss->phase = newPhase;
        switch (newPhase) {
            case BOSS_PHASE_1:
                boss->attackInterval = 3.0f;
                boss->orbInterval = 5.0f;
                break;
            case BOSS_PHASE_2:
                boss->attackInterval = 2.4f;
                boss->orbInterval = 4.5f;
                break;
            case BOSS_PHASE_3:
                boss->attackInterval = 2.0f;
                boss->orbInterval = 4.0f;
                break;
            case BOSS_PHASE_4:
                boss->attackInterval = 1.8f;
                boss->orbInterval = 3.5f;
                break;
        }
        // Phase 4 → trash talk 100% (final form moment)
        if (newPhase == BOSS_PHASE_4 && oldPhase != BOSS_PHASE_4) {
            boss->tauntText = TAUNTS_P4[rand() % 4];
            boss->tauntTimer = 3.5f;
        }
        
        // Reset skill round on phase change
        boss->skillRoundActive = false;
        boss->boomLaserSkillActive = false;
    }
}

// Cooldown per attack type (seconds)
static const float ATTACK_COOLDOWN_TABLE[9] = {
    0.5f,   // PROJECTILE - cho phép spam nhẹ
    4.0f,   // LASER - chiêu nguy hiểm, cooldown lâu
    3.0f,   // SLAM
    5.0f,   // HAZARD - chiếm sàn lâu
    3.0f,   // CLAW
    6.0f,   // BARRAGE - Phase 4 chiêu mạnh
    5.0f,   // RAIN
    5.0f,   // BOOM_CHAOTIC_LASERS
    6.0f    // BOOM_TRIPLE_TRACK_LASERS
};

static AttackType ChooseNewRoundAttack(Boss *boss) {
    AttackType allowed[8];
    int allowedCount = 0;

    if (boss->phase == BOSS_PHASE_2) {
        allowed[allowedCount++] = ATTACK_LASER;
        allowed[allowedCount++] = ATTACK_SLAM;
    } else if (boss->phase == BOSS_PHASE_3) {
        allowed[allowedCount++] = ATTACK_LASER;
        allowed[allowedCount++] = ATTACK_SLAM;
        allowed[allowedCount++] = ATTACK_CLAW;
        allowed[allowedCount++] = ATTACK_HAZARD;
        allowed[allowedCount++] = ATTACK_BOOM_CHAOTIC_LASERS;
        allowed[allowedCount++] = ATTACK_BOOM_TRIPLE_TRACK_LASERS;
    } else if (boss->phase == BOSS_PHASE_4) {
        allowed[allowedCount++] = ATTACK_LASER;
        allowed[allowedCount++] = ATTACK_SLAM;
        allowed[allowedCount++] = ATTACK_CLAW;
        allowed[allowedCount++] = ATTACK_HAZARD;
        allowed[allowedCount++] = ATTACK_BOOM_CHAOTIC_LASERS;
        allowed[allowedCount++] = ATTACK_BOOM_TRIPLE_TRACK_LASERS;
        allowed[allowedCount++] = ATTACK_RAIN;
    } else {
        return ATTACK_SLAM;
    }

    // Filter out lastRoundType to ensure alternation
    AttackType candidatePool[8];
    int candidateCount = 0;
    for (int i = 0; i < allowedCount; i++) {
        if (allowed[i] != boss->lastRoundType) {
            candidatePool[candidateCount++] = allowed[i];
        }
    }

    if (candidateCount > 0) {
        int idx = rand() % candidateCount;
        return candidatePool[idx];
    }

    return allowed[rand() % allowedCount];
}



static Vector2 lastPlayerPosForLaser = {640, 620.0f}; // Track player pos cho laser check

// === DECISION TREE: chọn chiêu THEO VỊ TRÍ player (kiểu Dylan dồn góc) ===
// Thay random thuần: boss "đọc" khoảng cách + vị trí player rồi đáp đúng chiêu trừng phạt.
// Ngưỡng khoảng cách (horizontal) player <-> boss.
#define DT_CLOSE_DIST   250.0f   // < 250: cận chiến -> Slam
#define DT_MID_DIST     550.0f   // 250..550: tầm trung -> Projectile fan; > 550 -> Laser
#define DT_EDGE_MARGIN  180.0f   // sát mép map -> dồn góc bằng Hazard/Claw



static void StartClawAttack(Boss *boss, Vector2 playerPos) {
    boss->clawActive = true;
    boss->clawWarningTime = CLAW_WARNING_TIME;
    boss->clawDuration = 0.0f;  // Sẽ active sau warning
    
    // 70% chọn zone player đang đứng (predict), 30% random
    int roll = rand() % 100;
    if (roll < 70) {
        // Predict zone player
        if (playerPos.x < 440.0f) boss->clawZone = CLAW_ZONE_LEFT;
        else if (playerPos.x < 840.0f) boss->clawZone = CLAW_ZONE_MIDDLE;
        else boss->clawZone = CLAW_ZONE_RIGHT;
    } else {
        // Random zone (player có thể né bằng cách đoán)
        boss->clawZone = (ClawZone)(rand() % 3);
    }
}

static void DoProjectileAttack(Boss *boss, Vector2 playerPos, ProjectileManager *pm) {
    // Spawn từ tay trái hoặc phải
    int hand = rand() % 2;
    Vector2 spawnPos = {
        boss->position.x + (hand == 0 ? LEFT_HAND_OFFSET_X : RIGHT_HAND_OFFSET_X) * boss->scale / 3.0f,
        boss->position.y + (hand == 0 ? LEFT_HAND_OFFSET_Y : RIGHT_HAND_OFFSET_Y) * boss->scale / 3.0f
    };

    Vector2 dir = NormalizeVec((Vector2){
        playerPos.x - spawnPos.x,
        playerPos.y - spawnPos.y
    });

    float speed = PROJECTILE_SPEED_BASE;

    switch (boss->phase) {
        case BOSS_PHASE_1:
            SpawnProjectile(pm, spawnPos, (Vector2){dir.x * speed, dir.y * speed}, 1);
            break;
        case BOSS_PHASE_2:
            speed *= 1.1f;
            SpawnProjectile(pm, spawnPos, (Vector2){dir.x * speed, dir.y * speed}, 1);
            {
                float angle = atan2f(dir.y, dir.x);
                float aL = angle - 0.3f, aR = angle + 0.3f;
                SpawnProjectile(pm, spawnPos, (Vector2){cosf(aL)*speed, sinf(aL)*speed}, 1);
                SpawnProjectile(pm, spawnPos, (Vector2){cosf(aR)*speed, sinf(aR)*speed}, 1);
            }
            break;
        case BOSS_PHASE_3:
            speed *= 1.15f;
            {
                float baseAngle = atan2f(dir.y, dir.x);
                for (int i = -1; i <= 1; i++) {
                    float a = baseAngle + i * 0.25f;
                    SpawnProjectile(pm, spawnPos, (Vector2){cosf(a)*speed, sinf(a)*speed}, 1);
                }
            }
            break;
        case BOSS_PHASE_4:
            // Phase 4: 5 viên fan (giảm từ 7) + speed thấp hơn (giảm từ 1.35)
            speed *= 1.25f;
            {
                float baseAngle = atan2f(dir.y, dir.x);
                for (int i = -2; i <= 2; i++) {
                    float a = baseAngle + i * 0.2f;
                    SpawnProjectile(pm, spawnPos, (Vector2){cosf(a)*speed, sinf(a)*speed}, 1);
                }
            }
            break;
    }
}

// Laser: gốc tại boss, lock vị trí player TRÊN PLATFORM (y=620, không lock trên không trung)
// Sau warning hết → tính direction từ vị trí player project xuống sàn → bắn cố định
// Tự tắt khi đầu xa chạm cạnh map
static void StartLaserAttack(Boss *boss, Vector2 playerPos) {
    boss->laserActive = true;
    boss->laserChargeTime = LASER_CHARGE_TIME;
    boss->laserDuration = LASER_DURATION;
    boss->laserStart = boss->position;
    // Project player xuống platform — dù player đang nhảy thì lock vẫn ở mặt sàn
    boss->laserEnd = (Vector2){ playerPos.x, GetSurfaceYAtX(playerPos.x) };
    boss->laserDirection = (Vector2){1, 0};  // Sẽ được lock khi warning kết thúc
}

static void StartSlamAttack(Boss *boss, Vector2 playerPos) {
    boss->slamActive = true;
    boss->slamWarningTime = SLAM_WARNING_TIME;  // Warning trước khi đập
    boss->slamTimer = 0.0f;  // Chưa bắt đầu shockwave
    boss->slamPos = (Vector2){playerPos.x, GetSurfaceYAtX(playerPos.x)};  // Vị trí target (player hiện tại)
    boss->shockwaveRadius = 0.0f;
}

static void StartHazardAttack(Boss *boss) {
    boss->hazardCount = 3 + rand() % 3;  // 3-5 hazards
    for (int i = 0; i < boss->hazardCount; i++) {
        float hx = 120.0f + (float)(rand() % 1040);
        boss->hazardPositions[i] = (Vector2){
            hx,  // Random X
            GetSurfaceYAtX(hx)  // Dynamically target floor or platform Y
        };
        // Stagger warning times: mỗi hazard cách nhau 0.3s để không cùng lúc
        boss->hazardWarningTime[i] = HAZARD_WARNING_TIME + i * 0.35f;
        boss->hazardActive[i] = false;
    }
}

// === PHASE 4 ONLY: BARRAGE - 360° radial burst ===
static void DoBarrageAttack(Boss *boss, ProjectileManager *pm) {
    // Bắn 12 projectile theo 360° xung quanh boss (cả 2 tay)
    Vector2 leftHand = {
        boss->position.x + LEFT_HAND_OFFSET_X * boss->scale / 3.0f,
        boss->position.y + LEFT_HAND_OFFSET_Y * boss->scale / 3.0f
    };
    Vector2 rightHand = {
        boss->position.x + RIGHT_HAND_OFFSET_X * boss->scale / 3.0f,
        boss->position.y + RIGHT_HAND_OFFSET_Y * boss->scale / 3.0f
    };
    
    float speed = PROJECTILE_SPEED_BASE * 1.1f;
    float twoPi = 6.2831853f;
    
    // 6 viên từ tay trái (offset góc 0)
    for (int i = 0; i < 6; i++) {
        float angle = (twoPi / 12.0f) * i;
        SpawnProjectile(pm, leftHand,
            (Vector2){cosf(angle) * speed, sinf(angle) * speed}, 1);
    }
    // 6 viên từ tay phải (offset góc 30°)
    for (int i = 0; i < 6; i++) {
        float angle = (twoPi / 12.0f) * i + (twoPi / 24.0f);
        SpawnProjectile(pm, rightHand,
            (Vector2){cosf(angle) * speed, sinf(angle) * speed}, 1);
    }
    
    // Shake hiệu ứng nổ
    boss->shakeTimer = 0.3f;
    boss->shakeIntensity = 15.0f;
}

// === PHASE 4 ONLY: RAIN - Mưa projectile từ trên trời ===
static void StartRainAttack(Boss *boss) {
    boss->rainActive = true;
    boss->rainWarningTime = 2.0f; // 2s warning
    boss->rainDuration = 0.0f;
}

void UpdateBoss(Boss *boss, Vector2 playerPos, ProjectileManager *pm, OrbManager *om, float dt, float *cameraShake) {
    // === DEATH SEQUENCE ===
    if (boss->defeated) {
        boss->deathTimer += dt;
        
        // Vẫn chạy animation khi chết (giật nhanh hơn)
        boss->frameTimer += dt;
        if (boss->frameTimer >= 0.05f) {
            boss->frameTimer = 0;
            boss->currentFrame = (boss->currentFrame + 1) % BOSS_TOTAL_FRAMES;
        }
        
        // Camera shake mạnh trong 2s đầu
        if (boss->deathTimer < 2.0f) {
            float intensity = 1.0f - (boss->deathTimer / 2.0f);
            *cameraShake = 15.0f * intensity;
        } else {
            *cameraShake = 0;
        }
        
        // Flash effect (3 lần đầu)
        if (boss->deathTimer < 1.5f) {
            boss->deathFlashTimer = sinf(boss->deathTimer * 15.0f) * 0.5f + 0.5f;
        } else {
            boss->deathFlashTimer = 0;
        }
        
        // Boss từ từ rơi xuống
        if (boss->deathTimer < 3.0f) {
            float fallProgress = boss->deathTimer / 3.0f;
            boss->position.y += 30.0f * dt * fallProgress;
        }
        
        return;
    }

    // Animation idle (luôn chạy)
    boss->frameTimer += dt;
    if (boss->frameTimer >= boss->animSpeed) {
        boss->frameTimer = 0;
        boss->currentFrame = (boss->currentFrame + 1) % BOSS_TOTAL_FRAMES;
    }

    // === PRE-INTRO SEQUENCE ===
    if (boss->state == BOSS_PRE_INTRO) {
        if (boss->preIntroTriggered) {
            boss->preIntroLightProgress += dt * 0.25f; // 4 seconds to light up
            if (boss->preIntroLightProgress >= 1.0f) {
                boss->preIntroLightProgress = 1.0f;
                boss->state = BOSS_INTRO;
                boss->introTimer = 0.0f;
            }
        } else {
            // Check if player is close to the beacon (within 40px in X distance)
            float dist = fabsf(playerPos.x - boss->beaconPos.x);
            if (dist < 40.0f) {
                boss->preIntroTriggered = true;
                boss->preIntroLightProgress = 0.0f;
            }
        }
        return;
    }

    // === INTRO SEQUENCE ===
    if (boss->state == BOSS_INTRO) {
        boss->introTimer += dt;
        float progress = boss->introTimer / INTRO_DURATION;
        if (progress > 1.0f) progress = 1.0f;
        
        // Boss rise CHẬM RÃII trong 45% intro đầu (0-7s), sau đó ở lại với floating effect
        float startY = 1200.0f;
        if (progress < 0.45f) {
            // Phase 1 (0-7s): rise chậm từ bên dưới màn hình
            float riseProgress = progress / 0.45f;
            float eased = 1.0f - powf(1.0f - riseProgress, 3.0f);  // ease-out cubic
            boss->position.y = startY + (boss->targetY - startY) * eased;
        } else {
            // Phase 2 (7-15s): boss visible, floating nhẹ + tăng intensity
            boss->position.y = boss->targetY + sinf(boss->introTimer * 2.0f) * 8.0f;
        }
        
        // Camera shake tăng dần (mạnh dần khi gần ROAR)
        float shakeIntensity = 2.0f + progress * 6.0f;
        *cameraShake = shakeIntensity;
        
        // Animation tăng tốc dần (boss "thức dậy")
        boss->animSpeed = 0.25f - progress * 0.18f;  // 0.25 → 0.07
        
        if (progress >= 1.0f) {
            boss->state = BOSS_ROAR;
            boss->roarTimer = ROAR_DURATION;
            boss->animSpeed = 0.02f;  // Animation x5 nhanh hơn khi ROAR
        }
        return;
    }

    // === ROAR SEQUENCE ===
    if (boss->state == BOSS_ROAR) {
        boss->roarTimer -= dt;
        // Screen shake mạnh
        *cameraShake = 8.0f * (boss->roarTimer / ROAR_DURATION);
        
        if (boss->roarTimer <= 0) {
            boss->state = BOSS_FIGHTING;
            *cameraShake = 0;
            boss->animSpeed = 0.1f;  // Trả về tốc độ bình thường khi vào fighting
        }
        return;
    }

    // === FAKE DEATH CUTSCENE (kiểu Dylan/Sans): 4 pha, ~8 giây ===
    // Pha 1 COLLAPSE 0.0-3.0s: boss gục, xám dần, im lặng -> lừa player tưởng thắng.
    // Pha 2 SILENCE  3.0-4.5s: khoảng lặng, T=4.0s mắt chớp đỏ báo trước.
    // Pha 3 REVIVAL  4.5-6.5s: glitch đỏ, boss giật + trồi dậy về chỗ cũ, rung mạnh.
    // Pha 4 DECLARE  6.5-8.0s: gầm + taunt -> sau đó vào TRUE_ENRAGE.
    // === FAKE DEATH + WALK-TO-DOOR + REVIVAL ===
    // Luồng: COLLAPSE (boss "chết") -> WAIT (player đi bộ ra cửa phải) -> REVIVAL (boss trồi lên) -> TRUE_ENRAGE.
    if (boss->state == BOSS_FAKE_DEATH) {
        // Animation boss vẫn chạy chậm trong lúc cutscene (gục)
        boss->frameTimer += dt;
        if (boss->frameTimer >= 0.18f) {
            boss->frameTimer = 0;
            boss->currentFrame = (boss->currentFrame + 1) % BOSS_TOTAL_FRAMES;
        }

        // GIAI ĐOẠN COLLAPSE: boss chìm xuống ~3s rồi "chết" -> mở cửa thoát.
        if (!boss->walkAwayDoorActive && boss->reviveWalkTimer <= 0.0f) {
            boss->fakeDeathTimer += dt;
            if (boss->fakeDeathTimer < 3.0f) {
                boss->position.y += 22.0f * dt;
                *cameraShake = 0.0f;
            } else {
                // Collapse xong -> hiện cửa thoát, trao quyền đi-bộ cho player (xử lý ở main.c).
                boss->walkAwayDoorActive = true;
                *cameraShake = 0.0f;
            }
            return;
        }

        // GIAI ĐOẠN WAIT: boss nằm gục, chờ player đi tới cửa.
        // main.c sẽ set walkAwayDoorActive=false + reviveWalkTimer=epsilon khi player tới cửa.
        if (boss->walkAwayDoorActive) {
            *cameraShake = 0.0f;
            return;
        }

        // GIAI ĐOẠN REVIVAL + DECLARE (đếm bằng reviveWalkTimer sau khi player tới cửa).
        boss->reviveWalkTimer += dt;
        float r = boss->reviveWalkTimer;
        if (r < 2.0f) {
            // REVIVAL: glitch + trồi dậy về targetY, rung mạnh dần.
            float k = r / 2.0f;
            boss->position.y += (boss->targetY - boss->position.y) * fminf(1.0f, dt * 6.0f);
            *cameraShake = 5.0f + k * 20.0f;
        } else if (r < 3.5f) {
            // DECLARE: đứng vững, gầm.
            boss->position.y = boss->targetY;
            *cameraShake = 6.0f;
            if (boss->tauntTimer <= 0.0f) {
                boss->tauntText = "CHUA XONG DAU!";
                boss->tauntTimer = 2.0f;
            } else {
                boss->tauntTimer -= dt;
            }
        } else {
            // -> TRUE_ENRAGE.
            boss->state = BOSS_TRUE_ENRAGE;
            boss->position.y = boss->targetY;
            boss->hp = 1;
            boss->maxHp = BOSS_MAX_HP;
            boss->phase = BOSS_PHASE_4;
            boss->fakeDeathTimer = 0.0f;
            boss->reviveWalkTimer = 0.0f;
            boss->tauntText = "CHET DI!";
            boss->tauntTimer = 3.0f;
            DoBarrageAttack(boss, pm);
            Vector2 hand = { boss->position.x, boss->position.y + 40.0f };
            SpawnParryOrb(om, hand, playerPos);
            boss->orbTimer = 0.0f;
            boss->orbInterval = 2.0f;
            *cameraShake = 25.0f;
        }
        return;
    }

    // === TRUE ENRAGE: 1 HP, bullet hell; parry orb kết liễu thật ===
    if (boss->state == BOSS_TRUE_ENRAGE) {
        UpdatePhase(boss); // giữ phase 4

        // Bắn barrage định kỳ tạo bullet hell, nhưng vẫn nhả orb để player parry.
        boss->attackTimer += dt;
        if (boss->attackTimer >= 1.6f) {
            boss->attackTimer = 0.0f;
            DoBarrageAttack(boss, pm);
        }
        boss->orbTimer += dt;
        if (boss->orbTimer >= boss->orbInterval) {
            boss->orbTimer = 0.0f;
            Vector2 hand = { boss->position.x, boss->position.y + 40.0f };
            SpawnParryOrb(om, hand, playerPos);
        }

        if (boss->tauntTimer > 0) {
            boss->tauntTimer -= dt;
            if (boss->tauntTimer < 0) boss->tauntTimer = 0;
        }
        if (boss->shakeTimer > 0) {
            boss->shakeTimer -= dt;
            if (boss->shakeTimer < 0) boss->shakeTimer = 0;
        }
        lastPlayerPosForLaser = playerPos;
        boss->hurtBox.x = boss->position.x - 100;
        boss->hurtBox.y = boss->position.y - 100;
        return;
    }

    // === DYING: Boss đứng yên, chờ ending music ===
    if (boss->state == BOSS_DYING) {
        // Chỉ chạy animation idle, không attack
        return;
    }

    // === FIGHTING (BOOM NODE MODE) ===
    // Phase tiến theo việc PHÁ ĐỦ 3 CỤC BOOM, không theo % máu nữa.
    if (!boss->boomsSpawned) {
        BoomSpawnPhase(boss);
        // Clear all active parry orbs to prevent immediate spawns or leftover orbs in the new phase
        for (int i = 0; i < MAX_ORBS; i++) {
            om->orbs[i].state = ORB_INACTIVE;
        }
        boss->orbActive = false;
    }
    UpdateBooms(boss, playerPos, om, dt);

    // Boss nhả orb VÀNG để parry. Phase 1 (0): 20s; các phase khác: 30-45s.
    boss->boomStaggerTimer += dt;
    if (boss->boomStaggerTimer >= boss->boomNextOrbDelay) {
        boss->boomStaggerTimer = 0.0f;
        if (boss->phase == BOSS_PHASE_1) {
            boss->boomNextOrbDelay = 20.0f;
        } else {
            boss->boomNextOrbDelay = 30.0f + (float)(rand() % 16); // 30-45s
        }
        Vector2 hand = { boss->position.x, boss->position.y + 40.0f };
        SpawnParryOrb(om, hand, playerPos);
    }

    // Phá đủ 3 cục boom -> boss DÍNH SÁT THƯƠNG -> sang phase mới (hoặc chết).
    if (boss->boomsSpawned && boss->boomsRemaining <= 0 && boss->phaseHitFlash <= 0.0f) {
        boss->phaseHitFlash = 0.6f;       // flash dính đòn
        boss->shakeTimer = 0.4f;
        boss->shakeIntensity = 25.0f;
        *cameraShake = 18.0f;
        boss->hp -= boss->maxHp / 4;      // clear 1 phase = -25% máu
        if (boss->hp < 0) boss->hp = 0;
        boss->boomsSpawned = false;       // spawn boom phase mới
        // Reset orb vàng: phase mới luôn có delay ngắn ngẫu nhiên (4-7s), không spawn ngay.
        boss->boomStaggerTimer = 0.0f;
        boss->boomNextOrbDelay = 4.0f + (float)(rand() % 4); // 4-7s
        if (boss->phase < BOSS_PHASE_4) {
            boss->phase = (BossPhase)((int)boss->phase + 1);
            boss->tauntText = TAUNTS_P4[rand() % 4];
            boss->tauntTimer = 2.5f;
        } else {
            // Hết phase 4 -> boss "chết" -> cutscene chết giả + đi ra cửa.
            boss->state = BOSS_FAKE_DEATH;
            boss->fakeDeathTimer = 0.0f;
            boss->fakeDeathDone = true;
            return;
        }
    }

    // Shake effect (khi bị đánh)
    if (boss->shakeTimer > 0) {
        boss->shakeTimer -= dt;
        if (boss->shakeTimer < 0) boss->shakeTimer = 0;
    }

    // Track player pos cho laser check
    lastPlayerPosForLaser = playerPos;

    // --- Attack timer ---
    bool anySkillActive = boss->laserActive || boss->slamActive || boss->clawActive || boss->rainActive || boss->boomLaserSkillActive;
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardActive[i] || boss->hazardWarningTime[i] > 0) {
            anySkillActive = true;
            break;
        }
    }
    bool wasSkillActive = anySkillActive;

    boss->attackTimer += dt;
    // Bật lại kỹ năng boss ở Phase 2, 3 & 4 (Laser, Slam, Hazard, Rain Orb, Claw, Barrage, Boom Lasers)
    if (boss->phase >= BOSS_PHASE_2 && boss->attackTimer >= boss->attackInterval && !anySkillActive) {
        boss->attackTimer = 0;
        
        // Start a new round of 5 casts if not active
        if (!boss->skillRoundActive) {
            boss->skillRoundType = ChooseNewRoundAttack(boss);
            boss->skillRoundActive = true;
            boss->skillRoundCastCount = 0;
        }
        
        AttackType atk = boss->skillRoundType;
        
        switch (atk) {
            case ATTACK_PROJECTILE:
                DoProjectileAttack(boss, playerPos, pm);
                break;
            case ATTACK_LASER:
                StartLaserAttack(boss, playerPos);
                break;
            case ATTACK_SLAM:
                StartSlamAttack(boss, playerPos);
                break;
            case ATTACK_HAZARD:
                StartHazardAttack(boss);
                break;
            case ATTACK_CLAW:
                StartClawAttack(boss, playerPos);
                break;
            case ATTACK_BARRAGE:
                DoBarrageAttack(boss, pm);
                break;
            case ATTACK_RAIN:
                StartRainAttack(boss);
                break;
            case ATTACK_BOOM_CHAOTIC_LASERS:
                BoomTriggerChaoticLasers(boss);
                break;
            case ATTACK_BOOM_TRIPLE_TRACK_LASERS:
                BoomTriggerTripleTrackLasers(boss);
                break;
        }
        
        // Increment cast count in the current round
        boss->skillRoundCastCount++;
        if (boss->skillRoundCastCount >= 5) {
            boss->skillRoundActive = false;
            boss->lastRoundType = boss->skillRoundType;
        }
        
        // Anti-spam: ghi nhận chiêu vừa dùng + set cooldown
        boss->lastAttack = atk;
        boss->attackCooldowns[(int)atk] = ATTACK_COOLDOWN_TABLE[(int)atk];
        // Trash talk khi tấn công (30% chance)
        TriggerTaunt(boss, TAUNTS_ATK, 5, 30);
    }

    // Decrement taunt timer
    if (boss->tauntTimer > 0) {
        boss->tauntTimer -= dt;
        if (boss->tauntTimer < 0) boss->tauntTimer = 0;
    }

    // Decrement cooldowns mỗi frame
    for (int i = 0; i < 9; i++) {
        if (boss->attackCooldowns[i] > 0) {
            boss->attackCooldowns[i] -= dt;
            if (boss->attackCooldowns[i] < 0) boss->attackCooldowns[i] = 0;
        }
    }
    
    // --- Update Claw ---
    if (boss->clawActive) {
        if (boss->clawWarningTime > 0) {
            boss->clawWarningTime -= dt;
            if (boss->clawWarningTime <= 0) {
                // Bắt đầu cào
                boss->clawDuration = CLAW_DURATION;
                boss->shakeTimer = 0.4f;
                boss->shakeIntensity = 20.0f;
            }
        } else if (boss->clawDuration > 0) {
            boss->clawDuration -= dt;
            if (boss->clawDuration <= 0) {
                boss->clawActive = false;
            }
        }
    }

    // --- Update Laser ---
    // Workflow: trigger → lock vị trí player → warning đứng yên 2s → lấy hướng player cuối → bắn cố định
    if (boss->laserActive) {
        // Gốc 1 luôn là vị trí boss
        boss->laserStart = boss->position;

        if (boss->laserChargeTime > 0) {
            // WARNING phase: laserEnd ĐỨNG YÊN tại vị trí đã lock (set trong StartLaserAttack)
            // KHÔNG track player — player có 2s để chạy
            float prev = boss->laserChargeTime;
            boss->laserChargeTime -= dt;

            // Khi warning vừa kết thúc → lấy vị trí player project xuống PLATFORM
            // → tính direction từ lockPos về groundPlayer (vẫn trên cùng mặt sàn)
            // → laser sẽ đi NGANG dọc platform, không bay vào không trung
            if (prev > 0 && boss->laserChargeTime <= 0) {
                Vector2 groundPlayer = { playerPos.x, GetSurfaceYAtX(playerPos.x) };
                Vector2 d = {
                    groundPlayer.x - boss->laserEnd.x,
                    groundPlayer.y - boss->laserEnd.y
                };
                float len = sqrtf(d.x*d.x + d.y*d.y);
                if (len < 1.0f) {
                    // Player đứng đúng vị trí lock → mặc định bắn ngang phải
                    boss->laserDirection = (Vector2){1.0f, 0.0f};
                } else {
                    boss->laserDirection = (Vector2){d.x / len, d.y / len};
                }
            }
        } else {
            // FIRING phase: laserEnd bắt đầu từ lockPos, kéo dài DẦN theo direction
            // KHÔNG teleport — laser lan ra từ vị trí cảnh báo về hướng player
            boss->laserDuration -= dt;

            // Kéo dài laserEnd theo direction mỗi frame
            boss->laserEnd.x += boss->laserDirection.x * LASER_EXTEND_SPEED * dt;
            boss->laserEnd.y += boss->laserDirection.y * LASER_EXTEND_SPEED * dt;

            // Tắt khi laserEnd chạm GẦN SÁT mép map
            if (boss->laserEnd.x <= 50.0f || boss->laserEnd.x >= 1230.0f ||
                boss->laserEnd.y <= 10.0f || boss->laserEnd.y >= 710.0f) {
                boss->laserActive = false;
            }

            // Hoặc hết duration (safety)
            if (boss->laserDuration <= 0) {
                boss->laserActive = false;
            }
        }
    }

    // --- Update Slam ---
    if (boss->slamActive) {
        if (boss->slamWarningTime > 0) {
            // Warning phase: hiện indicator, chưa gây damage
            boss->slamWarningTime -= dt;
            if (boss->slamWarningTime <= 0) {
                // Warning hết → bắt đầu shockwave
                boss->slamTimer = SLAM_DURATION;
                boss->shockwaveRadius = 0.0f;
                boss->shakeTimer = 0.4f;
                boss->shakeIntensity = 25.0f;
            }
        } else {
            // Shockwave expanding
            boss->slamTimer -= dt;
            float progress = 1.0f - (boss->slamTimer / SLAM_DURATION);
            boss->shockwaveRadius = progress * 300.0f;
            
            if (boss->slamTimer <= 0) {
                boss->slamActive = false;
                boss->shockwaveRadius = 0;
            }
        }
    }

    // --- Update Hazards ---
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardWarningTime[i] > 0) {
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] <= 0) {
                boss->hazardActive[i] = true;
            }
        } else if (boss->hazardActive[i]) {
            // Hazard active for 2 seconds then disappear
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] < -2.0f) {
                boss->hazardActive[i] = false;
            }
        }
    }

    // --- Update Rain ---
    if (boss->rainActive) {
        if (boss->rainWarningTime > 0) {
            boss->rainWarningTime -= dt;
            if (boss->rainWarningTime <= 0) {
                boss->rainWarningTime = 0.0f;
                boss->rainDuration = 3.0f;
            }
        } else if (boss->rainDuration > 0) {
            float prevDuration = boss->rainDuration;
            boss->rainDuration -= dt;
            
            // Spawn a projectile falling straight down every 0.3 seconds
            int prevTick = (int)(prevDuration / 0.3f);
            int currTick = (int)(boss->rainDuration / 0.3f);
            if (prevTick != currTick && boss->rainDuration > 0.0f) {
                float rx = 50.0f + (float)(rand() % 1180);
                SpawnProjectile(pm, (Vector2){rx, -20.0f}, (Vector2){0.0f, 350.0f}, 1);
            }
            
            if (boss->rainDuration <= 0) {
                boss->rainActive = false;
            }
        }
    }

    // --- Breathing Room Check ---
    bool isSkillActiveNow = boss->laserActive || boss->slamActive || boss->clawActive || boss->rainActive;
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardActive[i] || boss->hazardWarningTime[i] > 0) {
            isSkillActiveNow = true;
            break;
        }
    }
    if (wasSkillActive && !isSkillActiveNow) {
        boss->attackTimer = 0.0f;
    }

    // --- Initialize Statue System on First Frame Map is Ready ---
    if (!boss->statuesInitialized && arenaMap != NULL) {
        boss->mapStatueCount = 0;
        cute_tiled_layer_t* layer = arenaMap->layers;
        while (layer) {
            if (strcmp(layer->name.ptr, "statue") == 0) {
                cute_tiled_object_t* obj = layer->objects;
                while (obj && boss->mapStatueCount < MAX_MAP_STATUES) {
                    MapStatue *s = &boss->mapStatues[boss->mapStatueCount++];
                    s->id = obj->id;
                    s->hp = 1;        // 1 đòn là vỡ -> đánh nhanh, đỡ mệt
                    s->maxHp = 1;
                    s->active = false;           // starts INACTIVE until activated
                    s->respawnTimer = 0.0f;
                    s->animState = STATUE_INACTIVE;
                    s->animTimer = 0.0f;
                    s->currentFrame = 0;
                    s->hitbox = (Rectangle){
                        obj->x,
                        obj->y - obj->height,
                        obj->width,
                        obj->height
                    };
                    obj->visible = 1; // Initially show static Tiled image
                    obj = obj->next;
                }
                break;
            }
            layer = layer->next;
        }
        boss->statuesInitialized = true;
    }

    // --- STATUE GAMEPLAY DISABLED ---
    // Tượng không còn là cơ chế đánh boss nữa (đã bỏ theo yêu cầu).
    // Tượng giữ nguyên là vật trang trí tĩnh do Tiled vẽ (luôn INACTIVE, không kích hoạt).
    // Nguồn damage mới: parry orb do boss bắn ra (xem khối "PARRY ORB LOB" bên dưới).

    // --- PARRY ORB LOB: đã chuyển lên khối BOOM NODE MODE (spawn 30-45s/lần). ---

    // --- Update Statue Animation State Machine ---
    for (int i = 0; i < boss->mapStatueCount; i++) {
        MapStatue *s = &boss->mapStatues[i];
        s->animTimer += dt;

        if (s->animState == STATUE_INACTIVE) {
            // Countdown respawn timer
            if (s->respawnTimer > 0.0f) {
                s->respawnTimer -= dt;
                if (s->respawnTimer <= 0.0f) {
                    s->respawnTimer = 0.0f;
                    // Show again in tiled map
                    if (arenaMap) {
                        cute_tiled_layer_t* layer = arenaMap->layers;
                        while (layer) {
                            if (strcmp(layer->name.ptr, "statue") == 0) {
                                cute_tiled_object_t* obj = layer->objects;
                                while (obj) {
                                    if (obj->id == s->id) { obj->visible = 1; break; }
                                    obj = obj->next;
                                }
                                break;
                            }
                            layer = layer->next;
                        }
                    }
                }
            }
            s->animTimer = 0.0f;  // reset so activation starts clean

        } else if (s->animState == STATUE_ACTIVATING) {
            // Advance frame every STATUE_FRAME_TIME seconds
            float frameDuration = STATUE_FRAME_TIME;
            int newFrame = (int)(s->animTimer / frameDuration);
            if (newFrame >= STATUE_ACTIVATING_FRAMES) {
                // Activation complete → go ACTIVE
                s->animState = STATUE_ACTIVE;
                s->active = true;
                s->hp = 1;        // 1 đòn là vỡ -> đánh nhanh, đỡ mệt
                s->maxHp = 1;
                s->animTimer = 0.0f;
                s->currentFrame = 0;
            } else {
                s->currentFrame = newFrame;
            }

        } else if (s->animState == STATUE_ACTIVE) {
            // Loop the active animation
            float frameDuration = STATUE_FRAME_TIME * 1.5f;  // slightly slower for idle pulse
            s->currentFrame = (int)(s->animTimer / frameDuration) % STATUE_ACTIVE_FRAMES;

        } else if (s->animState == STATUE_SHATTERING) {
            float frameDuration = STATUE_FRAME_TIME;
            int newFrame = (int)(s->animTimer / frameDuration);
            if (newFrame >= STATUE_SHATTER_FRAMES) {
                // Shatter complete → go fully INACTIVE
                s->animState = STATUE_INACTIVE;
                s->active = false;
                s->animTimer = 0.0f;
                s->currentFrame = 0;
                s->respawnTimer = 5.0f;   // hồi nhanh hơn (15s -> 5s)
            } else {
                s->currentFrame = newFrame;
            }
        }
    }

    // Cập nhật hurtbox
    boss->hurtBox.x = boss->position.x - 100;
    boss->hurtBox.y = boss->position.y - 100;
}

void DrawBossBody(Boss *boss, Texture2D spriteSheet, float cameraOffsetY) {
    // Lazy load sprites on first draw
    LoadBossSprites();

    // === DEATH ANIMATION (làm lại cho gọn, điện ảnh) ===
    // Ý đồ: boss khựng -> nghiêng + CHÌM xuống mượt + mờ dần; 2-3 vòng xung kích ĐỀU
    // lan ra; vài cụm explosion sprite tuần tự trên thân; kết bằng 1 chớp trắng ngắn.
    if (boss->defeated) {
        float dp = boss->deathTimer / 4.0f;          // 0..1
        if (dp > 1.0f) dp = 1.0f;
        float ease = 1.0f - powf(1.0f - dp, 2.0f);   // ease-out cho mượt

        int frameIdx = boss->currentFrame;
        Rectangle source = { (float)frameIdx * BOSS_FRAME_W, 0, (float)BOSS_FRAME_W, (float)BOSS_FRAME_H };
        float destW = (float)BOSS_FRAME_W * boss->scale;
        float destH = (float)BOSS_FRAME_H * boss->scale;
        // Chìm xuống + nghiêng dần (không random giật).
        float sink = ease * 60.0f;
        float tilt = ease * 18.0f;
        Rectangle dest = {
            boss->position.x,
            boss->position.y + sink + cameraOffsetY,
            destW, destH
        };
        Vector2 origin = { destW / 2.0f, destH / 2.0f };

        // Tint: tím rực -> xám tối, mờ dần.
        unsigned char a = (unsigned char)((1.0f - dp) * 255);
        Color tint = {
            (unsigned char)(200 - ease * 120),
            (unsigned char)(60  + ease * 40),
            (unsigned char)(255 - ease * 180),
            a
        };
        DrawTexturePro(spriteSheet, source, dest, origin, tilt, tint);

        // 1 chớp trắng ngắn lúc khởi đầu (0.25s).
        if (boss->deathTimer < 0.25f) {
            float wf = 1.0f - (boss->deathTimer / 0.25f);
            DrawRectangle(-2000, -2000, 5000, 5000, (Color){255, 255, 255, (unsigned char)(wf * 220)});
        }

        // 3 vòng xung kích ĐỀU lan ra tuần tự (sạch, không random).
        for (int ring = 0; ring < 3; ring++) {
            float rt = boss->deathTimer - ring * 0.45f;
            if (rt > 0 && rt < 1.6f) {
                float radius = rt * 260.0f;
                float ra = (1.0f - rt / 1.6f);
                DrawCircleLines((int)boss->position.x, (int)(boss->position.y + cameraOffsetY),
                    radius, (Color){220, 120, 255, (unsigned char)(ra * 200)});
                DrawCircleLines((int)boss->position.x, (int)(boss->position.y + cameraOffsetY),
                    radius * 0.92f, (Color){255, 200, 255, (unsigned char)(ra * 120)});
            }
        }

        // Vài cụm explosion sprite nổ TUẦN TỰ trên thân (3 cụm, vị trí cố định).
        if (explosionTex.id > 0) {
            const Vector2 off[3] = { {-70, -40}, {60, -90}, {0, 30} };
            for (int e = 0; e < 3; e++) {
                float et = boss->deathTimer - e * 0.5f;
                if (et < 0 || et > 1.2f) continue;
                int expFrame = ((int)(et * 12.0f)) % EXPLOSION_FRAMES;
                Rectangle expSrc = { (float)(expFrame * EXPLOSION_FW), 0, (float)EXPLOSION_FW, (float)EXPLOSION_FH };
                float es = 2.6f;
                Rectangle expDst = {
                    boss->position.x + off[e].x - es*EXPLOSION_FW/2.0f,
                    boss->position.y + off[e].y - es*EXPLOSION_FH/2.0f + cameraOffsetY,
                    es*EXPLOSION_FW, es*EXPLOSION_FH
                };
                DrawTexturePro(explosionTex, expSrc, expDst, (Vector2){0,0}, 0,
                    (Color){255, 230, 255, (unsigned char)((1.0f - et/1.2f) * 255)});
            }
        }
        return;
    }

    // === FAKE-DEATH CUTSCENE VISUAL ===
    // 3 giai đoạn: COLLAPSE (gục xám) -> WAIT (nằm xám, chờ player ra cửa) -> REVIVAL (glitch + trồi dậy tím).
    if (boss->state == BOSS_FAKE_DEATH) {
        float destW = (float)BOSS_FRAME_W * boss->scale;
        float destH = (float)BOSS_FRAME_H * boss->scale;
        Rectangle src = { (float)boss->currentFrame * BOSS_FRAME_W, 0, (float)BOSS_FRAME_W, (float)BOSS_FRAME_H };
        Vector2 origin = { destW / 2.0f, destH / 2.0f };

        float shX = 0, shY = 0;
        float tilt = 0.0f;
        Color tint = WHITE;
        bool reviving = (boss->reviveWalkTimer > 0.0f);
        float r = boss->reviveWalkTimer;

        if (!reviving) {
            // COLLAPSE + WAIT: xám dần rồi nằm gục nghiêng (gục lâu hơn khi đang chờ player).
            float k = boss->fakeDeathTimer / 3.0f;
            if (k > 1.0f) k = 1.0f;
            unsigned char g = (unsigned char)(255 - k * 135);
            tint = (Color){ g, g, (unsigned char)(g + 10), 255 };
            tilt = k * 14.0f;
        } else if (r < 2.0f) {
            // REVIVAL: glitch nhấp nháy xám<->tím + giật ngang, dựng thẳng dần.
            float k = r / 2.0f;
            bool flick = ((int)(r * 30.0f)) % 2 == 0;
            tint = flick ? (Color){ 200, 50, 255, 255 } : (Color){ 140, 130, 150, 255 };
            shX = (float)(rand() % 16 - 8) * k;
            shY = (float)(rand() % 10 - 5) * k;
            tilt = 14.0f * (1.0f - k);
        } else {
            // DECLARE: tím rực, đứng thẳng, rung nhẹ.
            tint = (Color){ 210, 60, 255, 255 };
            shX = (float)(rand() % 8 - 4);
        }

        Rectangle dest = { boss->position.x + shX, boss->position.y + shY + cameraOffsetY, destW, destH };
        DrawTexturePro(spriteSheet, src, dest, origin, tilt, tint);

        // WAIT: mắt chớp đỏ khi cửa thoát đang mở (tín hiệu cú lừa sắp tới).
        if (boss->walkAwayDoorActive) {
            float blink = (sinf((float)GetTime() * 6.0f) + 1.0f) * 0.5f;
            float ex = boss->position.x, ey = boss->position.y - 40.0f + cameraOffsetY;
            DrawCircleV((Vector2){ex - 26, ey}, 5.0f, (Color){255, 30, 30, (unsigned char)(blink*200)});
            DrawCircleV((Vector2){ex + 26, ey}, 5.0f, (Color){255, 30, 30, (unsigned char)(blink*200)});
        }
        // REVIVAL: hào quang năng lượng tím + tia nứt quanh người.
        if (reviving) {
            float aura = (sinf(r * 10.0f) + 1.0f) * 0.5f;
            DrawCircleLines((int)boss->position.x, (int)(boss->position.y + cameraOffsetY),
                120.0f + aura * 40.0f, (Color){200, 60, 255, (unsigned char)(120 + aura*100)});
            for (int s = 0; s < 8; s++) {
                float a = s * (6.2831853f / 8.0f) + r * 4.0f;
                float d1 = 60.0f, d2 = 130.0f + aura * 30.0f;
                Vector2 p1 = { boss->position.x + cosf(a)*d1, boss->position.y + sinf(a)*d1 + cameraOffsetY };
                Vector2 p2 = { boss->position.x + cosf(a)*d2, boss->position.y + sinf(a)*d2 + cameraOffsetY };
                DrawLineEx(p1, p2, 2.5f, (Color){220, 120, 255, (unsigned char)(aura*200)});
            }
        }
        return;
    }

    float shakeX = 0, shakeY = 0;
    if (boss->shakeTimer > 0) {
        shakeX = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
        shakeY = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
    }

    int frameIdx = boss->currentFrame;
    float frameX = (float)frameIdx * BOSS_FRAME_W;

    Rectangle source = { frameX, 0, (float)BOSS_FRAME_W, (float)BOSS_FRAME_H };
    
    float destW = (float)BOSS_FRAME_W * boss->scale;
    float destH = (float)BOSS_FRAME_H * boss->scale;
    Rectangle dest = {
        boss->position.x + shakeX,
        boss->position.y + shakeY + cameraOffsetY,
        destW,
        destH
    };

    Vector2 origin = { destW / 2.0f, destH / 2.0f };

    // Tint theo phase
    Color tint = WHITE;
    if (boss->phase == BOSS_PHASE_2) tint = (Color){255, 200, 200, 255};
    if (boss->phase == BOSS_PHASE_3) tint = (Color){255, 100, 100, 255};
    if (boss->phase == BOSS_PHASE_4) tint = (Color){200, 50, 255, 255};  // Purple rage

    // Intro fade-in
    if (boss->state == BOSS_INTRO) {
        float alpha = boss->introTimer / INTRO_DURATION;
        if (alpha > 1.0f) alpha = 1.0f;
        tint.a = (unsigned char)(alpha * 255);
    }

    // Roar flash effect
    if (boss->state == BOSS_ROAR) {
        if ((int)(boss->roarTimer * 10) % 2 == 0) {
            tint = (Color){255, 255, 255, 255};
        } else {
            tint = (Color){255, 50, 50, 255};
        }
    }

    DrawTexturePro(spriteSheet, source, dest, origin, 0.0f, tint);

    // --- Draw Taunt Text (above boss) ---
    if (boss->tauntTimer > 0 && boss->state == BOSS_FIGHTING) {
        float alpha = (boss->tauntTimer > 1.0f) ? 1.0f : boss->tauntTimer;
        int textW = MeasureText(boss->tauntText, 20);
        DrawText(boss->tauntText,
            (int)(boss->position.x - textW/2),
            (int)(boss->position.y - 200),
            20, (Color){255, 255, 100, (unsigned char)(alpha * 230)});
    }

    // --- Draw Orb Charge Effect ---
    if (boss->orbChargeTimer > 0) {
        float progress = 1.0f - (boss->orbChargeTimer / ORB_CHARGE_TIME);
        float radius = 5.0f + progress * 20.0f;
        Color orbColor = (Color){255, 255, 0, (unsigned char)(progress * 200)};
        DrawCircleV(boss->orbSpawnPos, radius, orbColor);
        // Particles around charge
        for (int i = 0; i < 6; i++) {
            float angle = (float)i * (3.14159f * 2.0f / 6.0f) + boss->frameTimer * 5.0f;
            float dist = 30.0f * (1.0f - progress);
            Vector2 p = {
                boss->orbSpawnPos.x + cosf(angle) * dist,
                boss->orbSpawnPos.y + sinf(angle) * dist
            };
            DrawCircleV(p, 3.0f, (Color){255, 200, 0, (unsigned char)(progress * 150)});
        }
    }
}

void DrawBossSkills(Boss *boss) {
    if (boss->defeated) return;

    // Ensure sprites are loaded
    LoadBossSprites();

    // --- Draw Animated Statues ---
    for (int i = 0; i < boss->mapStatueCount; i++) {
        MapStatue *s = &boss->mapStatues[i];
        float cx = s->hitbox.x + s->hitbox.width / 2.0f;
        float cy = s->hitbox.y + s->hitbox.height / 2.0f;
        float hw = s->hitbox.width;
        float hh = s->hitbox.height;

        if (s->animState == STATUE_INACTIVE && s->respawnTimer > 0.0f) {
            // RECHARGING: show countdown ring and label
            float progress = 1.0f - (s->respawnTimer / 15.0f);
            float pulse = (sinf((float)GetTime() * 4.0f) + 1.0f) * 0.5f;
            DrawCircleV((Vector2){cx, cy}, 20.0f * progress + pulse * 5.0f,
                (Color){100, 200, 255, (unsigned char)(60 * progress)});
            DrawCircleLines((int)cx, (int)cy, 22.0f, (Color){100, 200, 255, (unsigned char)(100 + pulse * 80)});
            const char* text = "RECHARGING";
            int fs = 10;
            int textW = MeasureText(text, fs);
            DrawText(text, (int)(cx - textW / 2.0f), (int)(s->hitbox.y - 18.0f), fs, (Color){100, 200, 255, 160});

        } else if (s->animState == STATUE_ACTIVATING) {
            // ACTIVATING: expanding purple glow + rotating runes
            float progress = (float)s->currentFrame / (float)STATUE_ACTIVATING_FRAMES;
            float t = (float)GetTime();
            if (statueTex[0].id > 0) {
                // Draw from sprite sheet (horizontal strip)
                // Draw from 4-col grid sprite sheet using tileID 0-14 (sequential for activation)
                int tileID = s->currentFrame;  // frames 0-14 sequential
                int col = tileID % STATUE_SHEET_COLS;
                int row = tileID / STATUE_SHEET_COLS;
                Rectangle src = {
                    (float)(col * STATUE_TILE_SIZE),
                    (float)(row * STATUE_TILE_SIZE),
                    (float)STATUE_TILE_SIZE,
                    (float)STATUE_TILE_SIZE
                };
                Rectangle dst = { s->hitbox.x, s->hitbox.y, hw, hh };
                DrawTexturePro(statueTex[0], src, dst, (Vector2){0,0}, 0, WHITE);
            } else {
                // Fallback: pulsing purple aura growing outward
                float auraR = 20.0f + progress * 40.0f;
                unsigned char auraA = (unsigned char)(60 + progress * 120);
                DrawCircleV((Vector2){cx, cy}, auraR, (Color){160, 60, 255, auraA});
                DrawCircleV((Vector2){cx, cy}, auraR * 0.6f, (Color){200, 120, 255, (unsigned char)(auraA * 0.7f)});
                // Rotating rune particles
                for (int r = 0; r < 6; r++) {
                    float angle = t * 3.0f + r * (6.2831853f / 6.0f);
                    float dist = 30.0f + progress * 20.0f;
                    Vector2 rp = { cx + cosf(angle) * dist, cy + sinf(angle) * dist };
                    DrawCircleV(rp, 4.0f, (Color){220, 180, 255, (unsigned char)(150 + progress * 80)});
                }
                // Eyes glowing red as it activates
                float eyeAlpha = progress;
                DrawCircleV((Vector2){cx - 8, cy - 5}, 3.0f + progress * 2.0f,
                    (Color){255, 50, 50, (unsigned char)(eyeAlpha * 230)});
                DrawCircleV((Vector2){cx + 8, cy - 5}, 3.0f + progress * 2.0f,
                    (Color){255, 50, 50, (unsigned char)(eyeAlpha * 230)});
                // Body outline
                DrawRectangleLines((int)s->hitbox.x, (int)s->hitbox.y, (int)hw, (int)hh,
                    (Color){160, 60, 255, (unsigned char)(100 + progress * 100)});
            }

        } else if (s->animState == STATUE_ACTIVE) {
            // ACTIVE: glowing heart orb + red eyes + HP bar
            float t = (float)GetTime();
            if (statueTex[1].id > 0) {
                // Non-sequential tile IDs from statueindle.tsx
                int tileID = STATUE_ACTIVE_TILE_IDS[s->currentFrame];
                int col = tileID % STATUE_SHEET_COLS;
                int row = tileID / STATUE_SHEET_COLS;
                Rectangle src = {
                    (float)(col * STATUE_TILE_SIZE),
                    (float)(row * STATUE_TILE_SIZE),
                    (float)STATUE_TILE_SIZE,
                    (float)STATUE_TILE_SIZE
                };
                Rectangle dst = { s->hitbox.x, s->hitbox.y, hw, hh };
                DrawTexturePro(statueTex[1], src, dst, (Vector2){0,0}, 0, WHITE);
            } else {
                // Fallback: stone body + pulsing heart orb in center
                float pulse = (sinf(t * 3.0f) + 1.0f) * 0.5f;
                // Stone body
                DrawRectangle((int)s->hitbox.x, (int)s->hitbox.y, (int)hw, (int)hh,
                    (Color){80, 70, 90, 200});
                DrawRectangleLines((int)s->hitbox.x, (int)s->hitbox.y, (int)hw, (int)hh,
                    (Color){180, 80, 255, (unsigned char)(150 + pulse * 80)});
                // Glowing heart orb in center
                float orbR = 8.0f + pulse * 5.0f;
                DrawCircleV((Vector2){cx, cy}, orbR + 8.0f,
                    (Color){255, 60, 100, (unsigned char)(40 + pulse * 60)});
                DrawCircleV((Vector2){cx, cy}, orbR,
                    (Color){255, 80, 120, (unsigned char)(200 + pulse * 55)});
                DrawCircleV((Vector2){cx, cy}, orbR * 0.5f,
                    (Color){255, 200, 220, (unsigned char)(180 + pulse * 75)});
                // Red glowing eyes
                DrawCircleV((Vector2){cx - 8, cy - 10}, 4.0f,
                    (Color){255, 30, 30, (unsigned char)(180 + pulse * 75)});
                DrawCircleV((Vector2){cx + 8, cy - 10}, 4.0f,
                    (Color){255, 30, 30, (unsigned char)(180 + pulse * 75)});
            }
            // HP bar above statue (always shown in ACTIVE)
            float hpFrac = (s->maxHp > 0) ? (float)s->hp / s->maxHp : 0;
            int barW = (int)hw;
            int barH = 6;
            int barX = (int)s->hitbox.x;
            int barY = (int)(s->hitbox.y - 12);
            DrawRectangle(barX, barY, barW, barH, (Color){60, 20, 20, 200});
            DrawRectangle(barX, barY, (int)(barW * hpFrac), barH, (Color){255, 80, 80, 230});
            DrawRectangleLines(barX, barY, barW, barH, (Color){200, 100, 100, 180});

        } else if (s->animState == STATUE_SHATTERING) {
            // SHATTERING: disintegrating fragments flying outward
            float progress = (float)s->currentFrame / (float)STATUE_SHATTER_FRAMES;
            float t = (float)GetTime();
            if (statueTex[2].id > 0) {
                // Non-sequential tile IDs from statuedestruction.tsx (skips tile 11)
                int tileID = STATUE_SHATTER_TILE_IDS[s->currentFrame];
                int col = tileID % STATUE_SHEET_COLS;
                int row = tileID / STATUE_SHEET_COLS;
                Rectangle src = {
                    (float)(col * STATUE_TILE_SIZE),
                    (float)(row * STATUE_TILE_SIZE),
                    (float)STATUE_TILE_SIZE,
                    (float)STATUE_TILE_SIZE
                };
                float alpha = 1.0f - progress * 0.3f;  // Only slight fade at the end
                DrawTexturePro(statueTex[2], src,
                    (Rectangle){ s->hitbox.x, s->hitbox.y, hw, hh },
                    (Vector2){0,0}, 0, (Color){255,255,255,(unsigned char)(alpha*255)});
            } else {
                // Fallback: red-flash fragmentation effect
                float alpha = 1.0f - progress;
                unsigned char a = (unsigned char)(alpha * 200);
                // Fading body
                DrawRectangle((int)s->hitbox.x, (int)s->hitbox.y, (int)hw, (int)hh,
                    (Color){200, 60, 60, a});
                // Fragment particles flying outward
                for (int f = 0; f < 10; f++) {
                    float angle = f * (6.2831853f / 10.0f) + t * 5.0f;
                    float dist = progress * 60.0f + f * 5.0f;
                    Vector2 fp = { cx + cosf(angle) * dist, cy + sinf(angle) * dist };
                    float sz = 6.0f * (1.0f - progress);
                    if (sz > 0.5f) {
                        DrawRectangle((int)(fp.x - sz/2), (int)(fp.y - sz/2), (int)sz, (int)sz,
                            (Color){200, 120, 60, a});
                    }
                }
                // Flash ring
                DrawCircleLines((int)cx, (int)cy, 15.0f + progress * 50.0f,
                    (Color){255, 100, 50, (unsigned char)(alpha * 180)});
            }
        }
    }


    // --- Draw Laser ---
    if (boss->laserActive) {
        if (boss->laserChargeTime > 0) {
            // Charging: thin red line blinking
            float alpha = (sinf(boss->laserChargeTime * 20.0f) + 1.0f) * 0.5f;
            Color c = (Color){255, 0, 0, (unsigned char)(alpha * 150)};
            DrawLineEx(boss->laserStart, boss->laserEnd, 3.0f, c);
            // Warning circle at end
            DrawCircleV(boss->laserEnd, 20.0f, (Color){255, 0, 0, (unsigned char)(alpha * 100)});
        } else {
            // Firing: Draw animated laser texture
            if (laserTex.id > 0) {
                Vector2 diff = { boss->laserEnd.x - boss->laserStart.x, boss->laserEnd.y - boss->laserStart.y };
                float length = sqrtf(diff.x * diff.x + diff.y * diff.y);
                float angle = atan2f(diff.y, diff.x) * 180.0f / 3.14159265f; // RAD2DEG
                
                float elapsed = LASER_DURATION - boss->laserDuration;
                // Animation speed: 20 FPS (increased for faster animation). Clamped to final frame.
                int frameIdx = (int)(elapsed * 20.0f);
                if (frameIdx >= LASER_FRAMES) {
                    frameIdx = LASER_FRAMES - 1; // Hold on final frame
                }
                
                int col = frameIdx % LASER_COLS;
                int row = frameIdx / LASER_COLS;
                
                float thickness = 80.0f; // Make it thick and powerful!
                float extraLength = 100.0f;
                float drawLength = length + extraLength;
                
                // Keep the laser head (bottom part of spritesheet frame) unstretched:
                // Native dimensions of the head are square: width 300, height 300.
                float scale = thickness / (float)LASER_FW; 
                float headSourceHeight = 300.0f; 
                float headLength = headSourceHeight * scale; // Maintains aspect ratio (80.0f pixels)
                
                // Fade out laser towards the end of its duration
                float alpha = 1.0f;
                if (boss->laserDuration < 0.5f) {
                    alpha = boss->laserDuration / 0.5f; // Fade down to 0
                }
                Color tint = Fade(WHITE, alpha);
                
                // 1. Draw body part (stretched)
                float bodySourceHeight = (float)LASER_FH - headSourceHeight;
                float bodyLength = drawLength - headLength;
                if (bodyLength < 0.0f) bodyLength = 0.0f;
                
                Rectangle bodySource = { (float)(col * LASER_FW), (float)(row * LASER_FH), (float)LASER_FW, bodySourceHeight };
                Rectangle bodyDest = { boss->laserStart.x, boss->laserStart.y, thickness, bodyLength };
                Vector2 bodyOrigin = { thickness / 2.0f, 0.0f };
                DrawTexturePro(laserTex, bodySource, bodyDest, bodyOrigin, angle - 90.0f, tint);
                
                // 2. Draw head part (unstretched)
                if (length > 0.0f) {
                    Rectangle headSource = { (float)(col * LASER_FW), (float)(row * LASER_FH) + bodySourceHeight, (float)LASER_FW, headSourceHeight };
                    Vector2 dir = { diff.x / length, diff.y / length };
                    Vector2 headStart = { boss->laserStart.x + dir.x * bodyLength, boss->laserStart.y + dir.y * bodyLength };
                    Rectangle headDest = { headStart.x, headStart.y, thickness, headLength };
                    Vector2 headOrigin = { thickness / 2.0f, 0.0f };
                    DrawTexturePro(laserTex, headSource, headDest, headOrigin, angle - 90.0f, tint);
                }
            } else {
                // Fallback Firing: thick beam
                float pulse = (sinf(boss->laserDuration * 15.0f) + 1.0f) * 0.5f;
                float width = 15.0f + pulse * 10.0f;
                
                // Extend the fallback beam as well
                Vector2 diff = { boss->laserEnd.x - boss->laserStart.x, boss->laserEnd.y - boss->laserStart.y };
                float length = sqrtf(diff.x * diff.x + diff.y * diff.y);
                float drawLength = length + 100.0f;
                Vector2 dir = { diff.x / length, diff.y / length };
                Vector2 extendedEnd = { boss->laserStart.x + dir.x * drawLength, boss->laserStart.y + dir.y * drawLength };
                
                float alpha = 1.0f;
                if (boss->laserDuration < 0.5f) {
                    alpha = boss->laserDuration / 0.5f;
                }
                
                DrawLineEx(boss->laserStart, extendedEnd, width, Fade((Color){255, 50, 50, 230}, alpha));
                DrawLineEx(boss->laserStart, extendedEnd, width * 0.5f, Fade((Color){255, 200, 200, 200}, alpha));
                DrawCircleV(boss->laserStart, 20.0f, Fade((Color){255, 100, 100, 150}, alpha));
            }
        }
    }

    // --- Draw Slam Warning + Shockwave ---
    if (boss->slamActive) {
        if (boss->slamWarningTime > 0) {
            // WARNING PHASE: vòng tròn nhấp nháy tại vị trí sẽ đập
            float blink = (sinf(boss->slamWarningTime * 12.0f) + 1.0f) * 0.5f;
            float warningProgress = 1.0f - (boss->slamWarningTime / SLAM_WARNING_TIME);
            float radius = 50.0f + warningProgress * 250.0f;  // Vòng tròn mở rộng dần
            
            // Vòng tròn warning đỏ
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, radius,
                (Color){255, 50, 0, (unsigned char)(blink * 200)});
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, radius * 0.6f,
                (Color){255, 100, 0, (unsigned char)(blink * 150)});
            
            // Dấu X tại center
            float crossSize = 20.0f + warningProgress * 15.0f;
            DrawLineEx(
                (Vector2){boss->slamPos.x - crossSize, boss->slamPos.y - crossSize},
                (Vector2){boss->slamPos.x + crossSize, boss->slamPos.y + crossSize},
                3.0f, (Color){255, 255, 0, (unsigned char)(blink * 255)});
            DrawLineEx(
                (Vector2){boss->slamPos.x + crossSize, boss->slamPos.y - crossSize},
                (Vector2){boss->slamPos.x - crossSize, boss->slamPos.y + crossSize},
                3.0f, (Color){255, 255, 0, (unsigned char)(blink * 255)});
            
            // Text "!"
            DrawText("!", (int)boss->slamPos.x - 8, (int)boss->slamPos.y - 50, 40,
                (Color){255, 255, 0, (unsigned char)(blink * 255)});
        } else {
            // ACTIVE PHASE: shockwave expanding
            float alpha = boss->slamTimer / SLAM_DURATION;
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, boss->shockwaveRadius, 
                (Color){255, 150, 0, (unsigned char)(alpha * 255)});
            DrawCircleLines((int)boss->slamPos.x, (int)boss->slamPos.y, boss->shockwaveRadius * 0.7f, 
                (Color){255, 200, 50, (unsigned char)(alpha * 200)});
            // Impact point
            DrawCircleV(boss->slamPos, 15.0f * alpha, (Color){255, 100, 0, (unsigned char)(alpha * 200)});
        }
    }

    // --- Draw Claw Attack ---
    if (boss->clawActive) {
        // Xác định vùng X theo zone
        float zoneX = 20.0f, zoneW = 420.0f;
        if (boss->clawZone == CLAW_ZONE_MIDDLE) { zoneX = 440.0f; zoneW = 400.0f; }
        else if (boss->clawZone == CLAW_ZONE_RIGHT) { zoneX = 840.0f; zoneW = 420.0f; }
        
        if (boss->clawWarningTime > 0) {
            // Warning: vùng nhấp nháy đỏ/vàng
            float blink = (sinf(boss->clawWarningTime * 12.0f) + 1.0f) * 0.5f;
            DrawRectangle((int)zoneX, 0, (int)zoneW, 720,
                (Color){255, 50, 0, (unsigned char)(blink * 60)});
            // Claw marks preview
            for (int c = 0; c < 3; c++) {
                float cx = zoneX + zoneW * 0.2f + c * zoneW * 0.3f;
                DrawLineEx((Vector2){cx, 100}, (Vector2){cx - 30, 600}, 4.0f,
                    (Color){255, 100, 0, (unsigned char)(blink * 120)});
            }
            DrawText("!", (int)(zoneX + zoneW/2 - 10), 300, 60, 
                (Color){255, 255, 0, (unsigned char)(blink * 255)});
        } else if (boss->clawDuration > 0) {
            // Active: vẽ splash to đùng đè lên vùng đỏ dọc từ trên xuống dưới màn hình
            float progress = 1.0f - (boss->clawDuration / CLAW_DURATION);
            if (progress < 0) progress = 0;
            if (progress > 1.0f) progress = 1.0f;
            int frameIdx = (int)(progress * 9.0f);
            if (frameIdx < 0) frameIdx = 0;
            if (frameIdx > 8) frameIdx = 8;
            
            Texture2D tex = splashTexs[frameIdx];
            if (tex.id > 0) {
                Rectangle source = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
                Rectangle dest = { zoneX, 720.0f, 720.0f, zoneW };
                DrawTexturePro(tex, source, dest, (Vector2){0,0}, -90.0f, WHITE);
            }
        }
    }

    // --- Draw Atom Bomb Visual (overlay khi triggered) ---
    if (boss->atomTriggered) {
        float t = boss->atomTimer;
        // Whitewash flash full screen trong 0.5s đầu
        if (t < 0.5f) {
            float wAlpha = 1.0f - (t / 0.5f);
            DrawRectangle(-2000, -2000, 5000, 5000,
                (Color){255, 255, 255, (unsigned char)(wAlpha * 255)});
        }
        // Mushroom cloud explosion (Explosion.png scale to fullscreen)
        if (explosionTex.id > 0 && t < 4.0f) {
            int expFrame = ((int)(t * 6.0f)) % EXPLOSION_FRAMES;
            Rectangle expSrc = { (float)(expFrame * EXPLOSION_FW), 0, (float)EXPLOSION_FW, (float)EXPLOSION_FH };
            float scale = 8.0f + t * 3.0f;
            Rectangle expDst = {
                640.0f - scale * EXPLOSION_FW / 2.0f,
                360.0f - scale * EXPLOSION_FH / 2.0f,
                scale * EXPLOSION_FW,
                scale * EXPLOSION_FH
            };
            DrawTexturePro(explosionTex, expSrc, expDst, (Vector2){0,0}, 0,
                (Color){255, 200, 100, 255});
        }
        // Trash talk text giữa màn
        DrawText("ATOMIC ANNIHILATION", 640 - MeasureText("ATOMIC ANNIHILATION", 48)/2,
            300, 48, (Color){255, 50, 50, 255});
        DrawText("MAY GA VCL!", 640 - MeasureText("MAY GA VCL!", 32)/2,
            370, 32, (Color){255, 255, 100, 255});
    }

    // --- Draw Fight Timer (countdown đến atom) ---
    if (boss->state == BOSS_FIGHTING && boss->fightTimer < ATOM_TRIGGER_TIME) {
        float remaining = ATOM_TRIGGER_TIME - boss->fightTimer;
        int mins = (int)(remaining / 60);
        int secs = (int)remaining % 60;
        char timerBuf[16];
        snprintf(timerBuf, sizeof(timerBuf), "%02d:%02d", mins, secs);
        Color timerColor = (remaining < 30.0f) ? (Color){255, 50, 50, 255} : (Color){255, 255, 100, 200};
        DrawText(timerBuf, 1280/2 - 30, 90, 24, timerColor);
    }

    // --- Draw Rain Warning Overlay (Upper Screen) ---
    if (boss->rainActive && boss->rainWarningTime > 0) {
        float alphaPulse = (sinf(boss->rainWarningTime * 15.0f) + 1.0f) * 0.5f;
        
        // Semi-transparent red overlay on upper half screen
        DrawRectangle(0, 0, 1280, 360, (Color){255, 0, 0, (unsigned char)(alphaPulse * 80)});
        
        // Flashing orange divider line at y = 360
        DrawLineEx((Vector2){0, 360}, (Vector2){1280, 360}, 4.0f, (Color){255, 100, 0, (unsigned char)(alphaPulse * 200)});
        
        // Warning alerts text
        const char *warnText = "WARNING: ORBITAL BOMBARDMENT!";
        int fontSize = 36;
        int textW = MeasureText(warnText, fontSize);
        DrawText(warnText, 640 - textW/2, 160, fontSize, (Color){255, 255, 255, (unsigned char)(alphaPulse * 255)});
        
        // Warning Symbol
        const char *exclText = "!";
        int exclSize = 60;
        int exclW = MeasureText(exclText, exclSize);
        DrawText(exclText, 640 - exclW/2, 80, exclSize, (Color){255, 100, 0, (unsigned char)(alphaPulse * 255)});
    }
}

void DrawBoss(Boss *boss, Texture2D spriteSheet, float cameraOffsetY) {
    DrawBossBody(boss, spriteSheet, cameraOffsetY);
    DrawBossSkills(boss);
}

void BossTakeDamage(Boss *boss, int damage) {
    // Trong TRUE_ENRAGE: chỉ cần trúng 1 phát parry là chết thật.
    if (boss->state == BOSS_TRUE_ENRAGE) {
        boss->hp = 0;
        boss->state = BOSS_DYING;
        boss->shakeTimer = 0.5f;
        boss->shakeIntensity = 40.0f;
        return;
    }

    boss->hp -= damage;
    if (boss->hp <= 0) {
        boss->hp = 0;
        if (!boss->fakeDeathDone) {
            // === CÚ LỪA GIẢ CHẾT (Dylan twist) ===
            // Lần đầu HP=0 KHÔNG chết — chuyển sang giả chết, sẽ hồi sinh 1 HP.
            boss->fakeDeathDone = true;
            boss->state = BOSS_FAKE_DEATH;
            boss->fakeDeathTimer = 0.0f;
        } else {
            // Đã qua màn giả chết (đang ở TRUE_ENRAGE đã xử lý ở trên) — chết thật.
            boss->state = BOSS_DYING;
        }
    }
    boss->shakeTimer = 0.3f;
    boss->shakeIntensity = 30.0f;
    // Trash talk khi bị đánh (50% chance)
    TriggerTaunt(boss, TAUNTS_HIT, 4, 50);
}

bool CheckPlayerInShockwave(Vector2 playerPos, Vector2 slamPos, float radius) {
    float dx = playerPos.x - slamPos.x;
    float dy = playerPos.y - slamPos.y;
    float dist = sqrtf(dx*dx + dy*dy);
    // Player bị damage nếu trong vùng shockwave (ring)
    return (dist < radius && dist > radius - 40.0f);
}

bool CheckPlayerInLaser(Vector2 playerPos, Vector2 laserStart, Vector2 laserEnd, float width) {
    // Tính khoảng cách từ player đến đường laser
    Vector2 d = { laserEnd.x - laserStart.x, laserEnd.y - laserStart.y };
    float len = sqrtf(d.x*d.x + d.y*d.y);
    if (len < 1.0f) return false;
    
    d.x /= len; d.y /= len;
    Vector2 toPlayer = { playerPos.x - laserStart.x, playerPos.y - laserStart.y };
    float proj = toPlayer.x * d.x + toPlayer.y * d.y;
    
    if (proj < 0 || proj > len) return false;
    
    float perpDist = fabsf(toPlayer.x * (-d.y) + toPlayer.y * d.x);
    return perpDist < width / 2.0f;
}

bool CheckPlayerInClawZone(Vector2 playerPos, ClawZone zone) {
    // Zones: LEFT (20-440), MIDDLE (440-840), RIGHT (840-1260)
    switch (zone) {
        case CLAW_ZONE_LEFT:
            return playerPos.x >= 20.0f && playerPos.x < 440.0f;
        case CLAW_ZONE_MIDDLE:
            return playerPos.x >= 440.0f && playerPos.x < 840.0f;
        case CLAW_ZONE_RIGHT:
            return playerPos.x >= 840.0f && playerPos.x <= 1260.0f;
    }
    return false;
}


// CHỈ tượng đang ĐƯỢC KÍCH HOẠT (ACTIVE) mới là vật cản đặc.
// Tượng đá tĩnh (INACTIVE), đang hiện lên (ACTIVATING), đang vỡ (SHATTERING)
// hay đang chờ respawn đều CHO player đi xuyên qua tự do.
static bool StatueIsSolid(const MapStatue *s) {
    return (s->animState == STATUE_ACTIVE);
}

// Đẩy player ra khỏi thân tượng ACTIVE theo phương ngang.
// position là điểm chân (bottom-center) của player; halfWidth/bodyHeight là kích thước thân.
void BossResolveStatueCollision(Boss *boss, Vector2 *position, float halfWidth, float bodyHeight) {
    if (!position) return;
    for (int i = 0; i < boss->mapStatueCount; i++) {
        MapStatue *s = &boss->mapStatues[i];
        if (!StatueIsSolid(s)) continue;

        // Khối chắn HẸP: chỉ phần lõi giữa-dưới của tượng (50% bề ngang, 35% chiều cao dưới),
        // để player vẫn lách sát được và không bị "tường vô hình" rộng chặn đường.
        float coreW = s->hitbox.width * 0.50f;
        float baseX = s->hitbox.x + (s->hitbox.width - coreW) * 0.5f;
        float solidTop = s->hitbox.y + s->hitbox.height * 0.65f;
        Rectangle base = {
            baseX,
            solidTop,
            coreW,
            s->hitbox.y + s->hitbox.height - solidTop
        };

        float playerLeft   = position->x - halfWidth;
        float playerRight  = position->x + halfWidth;
        float playerTop    = position->y - bodyHeight;
        float playerBottom = position->y;

        // AABB overlap?
        bool overlap = (playerRight > base.x) && (playerLeft < base.x + base.width) &&
                       (playerBottom > base.y) && (playerTop < base.y + base.height);
        if (!overlap) continue;

        // Đẩy theo phương ngang với khoảng cách nhỏ nhất (min displacement).
        float pushRight = (base.x + base.width) - playerLeft;  // đẩy player sang phải
        float pushLeft  = playerRight - base.x;                // đẩy player sang trái
        if (pushRight < pushLeft) {
            position->x += pushRight;
        } else {
            position->x -= pushLeft;
        }
    }
}

void BossCheckStatueHits(Boss *boss, Rectangle attackBox, Sound hitSfx, Sound shatterSfx, OrbManager *om) {
    for (int i = 0; i < boss->mapStatueCount; i++) {
        MapStatue *s = &boss->mapStatues[i];
        // Only hittable when ACTIVE
        if (s->animState != STATUE_ACTIVE) continue;

        // Hitbox "rộng tay" hơn để đòn đánh dễ trúng: nới rộng vùng nhận đòn của tượng.
        Rectangle hitArea = {
            s->hitbox.x - 24.0f,
            s->hitbox.y - 16.0f,
            s->hitbox.width + 48.0f,
            s->hitbox.height + 32.0f
        };

        if (CheckCollisionRecs(attackBox, hitArea)) {
            s->hp--;
            if (s->hp <= 0) {
                s->hp = 0;
                s->active = false;
                // Transition to shattering animation
                s->animState = STATUE_SHATTERING;
                s->animTimer = 0.0f;
                s->currentFrame = 0;
                // respawnTimer set when SHATTERING completes (in state machine)
                
                if (IsSoundReady(shatterSfx)) {
                    PlaySound(shatterSfx);
                }
                
                // Spawn orb at statue center immediately on shatter
                SpawnOrb(om, (Vector2){ s->hitbox.x + s->hitbox.width / 2.0f, s->hitbox.y + s->hitbox.height / 2.0f });
            } else {
                if (IsSoundReady(hitSfx)) {
                    PlaySound(hitSfx);
                }
            }
        }
    }
}

float BossGetGroundY(float x) {
    return GetSurfaceYAtX(x);
}
