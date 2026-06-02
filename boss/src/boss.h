#ifndef BOSS_H
#define BOSS_H

#include "raylib.h"

extern Font gGameFont;

#ifndef DrawText
#define DrawText(text, x, y, size, ...) DrawTextEx(gGameFont, text, (Vector2){(float)(x), (float)(y)}, (float)(size), 1.0f, __VA_ARGS__)
#endif

#ifndef MeasureText
#define MeasureText(text, size) ((int)MeasureTextEx(gGameFont, text, (float)(size), 1.0f).x)
#endif
#include "projectile.h"
#include "orb.h"
#include "cute_tiled.h"
#include <stdbool.h>

#define MAX_MAP_STATUES 8

// Statue animation states
typedef enum {
    STATUE_INACTIVE,      // Lifeless stone statue, drawn by Tiled map (visible = 1)
    STATUE_ACTIVATING,    // Transitioning (playing Activating animation strip)
    STATUE_ACTIVE,        // Fully active and ready to be hit (playing Active Looping strip)
    STATUE_SHATTERING     // Shattering upon 0 HP (playing Shatter animation strip)
} StatueAnimState;

typedef struct {
    int id;
    int hp;
    int maxHp;
    bool active;          // true when in STATUE_ACTIVE state
    Rectangle hitbox;
    float respawnTimer;
    // Animation
    StatueAnimState animState;
    float animTimer;      // Accumulates dt for frame stepping
    int currentFrame;     // Current frame index in the sprite sheet
} MapStatue;

#define BOSS_MAX_HP 75

// === BOOM NODE (cục boom sinh đòn, phá bằng parry orb) ===
#define BOOM_PER_PHASE 3
typedef enum {
    BOOM_START,   // animation hiện lên (part1)
    BOOM_LOOP,    // đứng phát đòn (part2 loop)
    BOOM_END,     // nổ biến mất (part3)
    BOOM_GONE     // đã biến mất
} BoomState;

typedef struct {
    Vector2 position;
    BoomState state;
    float animTimer;
    int currentFrame;
    float attackTimer;      // đếm tới đòn kế tiếp
    float telegraphTimer;   // >0: đang hiện ring cảnh báo (2s) trước khi bắn
    int attackKind;         // kiểu đòn theo phase + vị trí cục
    float scale;
    int ringSpawnsRemaining;
    float ringSpawnTimer;
    float lastGapAngle;
} BoomNode;

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
    BOSS_PRE_INTRO,     // Mèo đứng trên map tối, chưa có quái, cần đi đến điểm sáng
    BOSS_INTRO,         // Boss đang xuất hiện từ dưới lên
    BOSS_ROAR,          // Boss hét + screen shake
    BOSS_FIGHTING,      // Đang chiến đấu
    BOSS_FAKE_DEATH,    // HP=0 lần đầu: GIẢ CHẾT (cú lừa kiểu Dylan)
    BOSS_TRUE_ENRAGE,   // Hồi sinh 1 HP + bullet hell, parry orb cuối = kết liễu thật
    BOSS_DYING,         // HP=0 thật, đứng yên chờ nhạc ending phát hết
    BOSS_DEFEATED       // Sau khi ending hết: explosion + fade
} BossState;

typedef enum {
    ATTACK_PROJECTILE,  // Bắn projectile thường (P1+)
    ATTACK_LASER,       // Bắn laser beam (P2+)
    ATTACK_SLAM,        // Đập tay xuống tạo shockwave (P2+)
    ATTACK_HAZARD,      // Spawn hazard obstacles (P3+)
    ATTACK_CLAW,        // Vuốt cào 1 khu vực (P3+)
    ATTACK_BARRAGE,     // P4 ONLY: 360° radial burst 12 projectiles
    ATTACK_RAIN,        // P4 ONLY: Rain 6 projectiles từ trên trời
    ATTACK_BOOM_CHAOTIC_LASERS,      // Phase 3+: Chaotic lasers from boom nodes
    ATTACK_BOOM_TRIPLE_TRACK_LASERS  // Phase 3+: Triple tracking lasers from boom nodes
} AttackType;

typedef enum {
    CLAW_ZONE_LEFT,     // Khu vực bên trái
    CLAW_ZONE_MIDDLE,   // Khu vực giữa
    CLAW_ZONE_RIGHT     // Khu vực bên phải
} ClawZone;

typedef struct {
    Vector2 position;
    Rectangle hurtBox;
    int hp;
    int maxHp;
    BossPhase phase;
    BossState state;
    bool defeated;

    // Intro sequence
    float introTimer;
    float targetY;          // Vị trí Y cuối cùng
    float roarTimer;
    
    // Attack system
    float attackTimer;
    float attackInterval;
    AttackType nextAttack;
    AttackType lastAttack;          // Lưu chiêu vừa dùng (anti-spam: không cho ra trùng)
    float attackCooldowns[9];       // Cooldown per attack (PROJECTILE, LASER, SLAM, HAZARD, CLAW, BARRAGE, RAIN, BOOM_CHAOTIC, BOOM_TRIPLE)

    // Trash talk system
    float tauntTimer;               // Thời gian hiện text taunt
    const char *tauntText;          // Câu taunt hiện tại
    
    // Orb system (chỉ spawn 1 orb tại 1 thời điểm)
    float orbTimer;
    float orbInterval;
    bool orbActive;         // Có orb đang active không
    float orbChargeTimer;   // Thời gian charge trước khi spawn
    Vector2 orbSpawnPos;    // Vị trí spawn (tay trái hoặc phải)
    
    // Statue system
    MapStatue mapStatues[MAX_MAP_STATUES];
    int mapStatueCount;
    bool statuesInitialized;
    
    // Laser attack
    bool laserActive;
    float laserChargeTime;
    float laserDuration;
    Vector2 laserStart;
    Vector2 laserEnd;
    Vector2 laserDirection;  // Hướng cố định sau khi lock (normalize)
    
    // Slam attack (có warning trước khi đập)
    bool slamActive;
    float slamWarningTime;  // Thời gian warning trước khi đập
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

    // Rain attack (Phase 4+)
    bool rainActive;
    float rainWarningTime;
    float rainDuration;

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

    // Fake-death twist (kiểu Dylan): HP=0 lần đầu không chết, hồi sinh 1 HP
    bool fakeDeathDone;     // Đã dùng cú lừa giả chết chưa
    float fakeDeathTimer;   // Timeline cho chuỗi giả chết

    // Pre-intro cutscene fields
    float preIntroLightProgress; // 0.0f (dark) to 1.0f (fully lit)
    bool preIntroTriggered;      // locked cat, lighting up
    Vector2 beaconPos;           // location of the glowing beacon

    // === BOOM NODE SYSTEM (cốt lõi mới) ===
    BoomNode booms[BOOM_PER_PHASE];
    int boomsRemaining;          // số cục boom còn lại trong phase hiện tại
    bool boomsSpawned;           // đã spawn boom cho phase này chưa
    float boomStaggerTimer;      // spawn orb vàng (để parry) cách nhau 30-45s
    float boomNextOrbDelay;      // delay ngẫu nhiên hiện tại
    float phaseHitFlash;         // >0: boss vừa "dính sát thương" sau khi phá đủ 3 boom
    bool walkAwayDoorActive;     // (cutscene) cửa thoát đang hiện cho player đi ra
    float reviveWalkTimer;       // (cutscene) đếm thời gian giai đoạn đi ra cửa

    // Skill sequencing & special laser logic
    AttackType skillRoundType;
    int skillRoundCastCount;
    bool skillRoundActive;
    AttackType lastRoundType;
    bool boomLaserSkillActive;
    int activeBoomSkillType;
    float boomSkillTimer;
} Boss;

// Boom system API
void BoomSpawnPhase(Boss *boss);
void UpdateBooms(Boss *boss, Vector2 playerPos, OrbManager *om, float dt);
void DrawBooms(Boss *boss);
int BoomNearestActive(Boss *boss, Vector2 pos); // trả index cục boom gần nhất còn sống, -1 nếu hết
void BoomHit(Boss *boss, int index);            // phá 1 cục (orb parry đập trúng)
bool CheckPlayerInBoomRings(Boss *boss, Vector2 playerPos); // va chạm ring damage-orb
void UnloadBoomAssets(void);
void BoomTriggerChaoticLasers(Boss *boss);
void BoomTriggerTripleTrackLasers(Boss *boss);

void InitBoss(Boss *boss, Vector2 startPos, Vector2 targetPos);
void BossSetArenaMap(cute_tiled_map_t *map, float offsetY, float fallbackGroundY);
void UpdateBoss(Boss *boss, Vector2 playerPos, ProjectileManager *pm, OrbManager *om, float dt, float *cameraShake);
void DrawBoss(Boss *boss, Texture2D spriteSheet, float cameraOffsetY);
void DrawBossBody(Boss *boss, Texture2D spriteSheet, float cameraOffsetY);
void DrawBossSkills(Boss *boss);
void BossTakeDamage(Boss *boss, int damage);
void BossCheckStatueHits(Boss *boss, Rectangle attackBox, Sound hitSfx, Sound shatterSfx, OrbManager *om);
void BossResolveStatueCollision(Boss *boss, Vector2 *position, float halfWidth, float bodyHeight);
bool CheckPlayerInShockwave(Vector2 playerPos, Vector2 slamPos, float radius);
bool CheckPlayerInLaser(Vector2 playerPos, Vector2 laserStart, Vector2 laserEnd, float width);
bool CheckPlayerInClawZone(Vector2 playerPos, ClawZone zone);
float BossGetGroundY(float x);
void UnloadBossAssets(void);

#endif // BOSS_H
