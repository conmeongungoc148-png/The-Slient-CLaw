#include "boss.h"
#include "boss_assets.h"
#include "collision.h"
#include <math.h>
#include <stdlib.h>

// === BOOM NODE SYSTEM ===
// 3 cục boom mỗi phase. Player lụm orb vàng -> orb bay lên đập cục boom gần nhất -> nổ.
// Phá đủ 3 cục -> boss dính sát thương -> sang phase mới.
//
// ĐÒN HẠI PLAYER theo phase:
//   - Phase 1: CẢ 3 cục bắn "ring damage-orb" (vòng orb đỏ có khe né).
//   - Phase >=2: CHỈ cục GIỮA (index 1) bắn ring orb đỏ gây sát thương.
//        + Cục TRÁI  (index 0): LASER BEAM (telegraph -> bắn 1 luồng thẳng).
//        + Cục PHẢI  (index 2): SHOCKWAVE PULSE (vòng xung kích lan ra - skill tự thiết kế).

// Sprite sheets (3 part), mỗi frame 128x128, xếp ngang.
static Texture2D boomStartTex = {0};   // part1: 8 frames
static Texture2D boomLoopTex  = {0};   // part2: 5 frames
static Texture2D boomEndTex   = {0};   // part3: 6 frames
static Texture2D ringOrbTex   = {0};   // ring damage orb asset
static bool boomLoaded = false;

#define BOOM_FRAME 128
#define BOOM_START_FRAMES 8
#define BOOM_LOOP_FRAMES  5
#define BOOM_END_FRAMES   6
#define BOOM_FRAME_TIME   0.09f

// Ring damage-orb (đòn hại player kiểu vòng tròn có khe né)
#define MAX_RING_ORBS 64
typedef struct {
    Vector2 pos;
    Vector2 vel;
    float radius;
    float holdTimer;   // >0: còn đứng yên quanh cục boom (telegraph)
    float life;        // tổng đời, để tự huỷ
    bool active;
} RingOrb;

static RingOrb ringOrbs[MAX_RING_ORBS] = {0};

// === SKILL STATE cho cục boom 2 bên (phase>=2), index theo cục boom ===
#define BEAM_TELEGRAPH_TIME 1.2f   // thời gian báo trước (vạch mảnh)
#define BEAM_FIRE_TIME      0.45f  // thời gian beam bắn (vạch dày + hitbox)
#define BEAM_WIDTH          48.0f  // bề rộng hitbox beam và tia vẽ

#define BOOM_LASER_FRAMES 8
#define BOOM_LASER_COLS   4
#define BOOM_LASER_FW     300
#define BOOM_LASER_FH     1309

static float   beamTelegraph[BOOM_PER_PHASE] = {0}; // >0: laser đang telegraph
static float   beamFire[BOOM_PER_PHASE]      = {0}; // >0: laser đang bắn
static Vector2 beamDir[BOOM_PER_PHASE]       = {0}; // hướng laser đã lock

static void ClearBoomSkill(int i) {
    if (i < 0 || i >= BOOM_PER_PHASE) return;
    beamTelegraph[i] = 0.0f;
    beamFire[i] = 0.0f;
}

static float GetBeamLength(Vector2 start, Vector2 dir) {
    float length = 0.0f;
    for (float t = 0.0f; t < 2000.0f; t += 10.0f) {
        Vector2 p = { start.x + dir.x * t, start.y + dir.y * t };
        float groundY = BossGetGroundY(p.x);
        if (p.y >= groundY) {
            length = t;
            break;
        }
    }
    if (length == 0.0f) length = 2000.0f;
    return length;
}

static void LoadBoomAssets(void) {
    if (boomLoaded) return;
    boomStartTex = LoadTexture(GetBossAssetPath("assets/effects/boom/part1(start)/sprite-sheet.png"));
    boomLoopTex  = LoadTexture(GetBossAssetPath("assets/effects/boom/part2(loop)/sprite-sheet.png"));
    boomEndTex   = LoadTexture(GetBossAssetPath("assets/effects/boom/part3(end)/sprite-sheet.png"));
    ringOrbTex   = LoadTexture(GetBossAssetPath("assets/effects/vfx/orbdamage/sprite-sheet.png"));
    boomLoaded = true;
}

void UnloadBoomAssets(void) {
    if (boomLoaded) {
        UnloadTexture(boomStartTex);
        UnloadTexture(boomLoopTex);
        UnloadTexture(boomEndTex);
        UnloadTexture(ringOrbTex);
        boomStartTex = (Texture2D){0};
        boomLoopTex = (Texture2D){0};
        boomEndTex = (Texture2D){0};
        ringOrbTex = (Texture2D){0};
        boomLoaded = false;
    }
    for (int i = 0; i < MAX_RING_ORBS; i++) ringOrbs[i].active = false;
    for (int i = 0; i < BOOM_PER_PHASE; i++) ClearBoomSkill(i);
}

static void SpawnRingOrb(Vector2 pos, Vector2 vel, float holdTimer) {
    for (int i = 0; i < MAX_RING_ORBS; i++) {
        if (!ringOrbs[i].active) {
            ringOrbs[i].pos = pos;
            ringOrbs[i].vel = vel;
            ringOrbs[i].radius = 11.0f;
            ringOrbs[i].holdTimer = holdTimer;
            ringOrbs[i].life = 0.0f;
            ringOrbs[i].active = true;
            return;
        }
    }
}

// Bắn 1 ring orb quanh cục boom: nhiều orb xếp vòng tròn, chừa 1 khe trống duy nhất.
// count: số orb trong vòng. gapAngle: vị trí khe (radian). gapW: độ rộng khe.
static void FireRing(Vector2 center, int count, float speed, float gapAngle, float gapW) {
    for (int i = 0; i < count; i++) {
        float ang = (6.2831853f / count) * i;
        // Chừa khe duy nhất
        float d1 = fabsf(ang - gapAngle);
        if (d1 > 3.14159f) d1 = 6.2831853f - d1;
        if (d1 < gapW) continue;
        Vector2 dir = { cosf(ang), sinf(ang) };
        // Orb spawn cách tâm 1 chút, đứng yên 2s rồi mới bay (holdTimer).
        Vector2 spawn = { center.x + dir.x * 36.0f, center.y + dir.y * 36.0f };
        Vector2 vel = { dir.x * speed, dir.y * speed };
        SpawnRingOrb(spawn, vel, 2.0f);
    }
}

// Vị trí 3 cục boom theo phase: cách xa nhau, gần boss, hơi cao.
// out[0]=trái, out[1]=giữa, out[2]=phải.
static void BoomPositions(int phase, Vector2 out[BOOM_PER_PHASE]) {
    float cy = 230.0f;                       // hơi cao trên đầu boss
    float spread = 250.0f + phase * 25.0f;   // phase cao -> xa hơn
    float cx = 640.0f;
    out[0] = (Vector2){ cx - spread, cy + 20.0f };
    out[1] = (Vector2){ cx,          cy - 30.0f };
    out[2] = (Vector2){ cx + spread, cy + 20.0f };
}

void BoomSpawnPhase(Boss *boss) {
    LoadBoomAssets();
    Vector2 pos[BOOM_PER_PHASE];
    BoomPositions((int)boss->phase, pos);
    
    // Clear any active laser skills and active ring orbs on transition
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        ClearBoomSkill(i);
    }
    for (int i = 0; i < MAX_RING_ORBS; i++) {
        ringOrbs[i].active = false;
    }
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        b->position = pos[i];
        b->state = BOOM_START;
        b->animTimer = 0.0f;
        b->currentFrame = 0;
        // mỗi cục mỗi phase 1 kiểu đòn khác nhau
        b->attackKind = (i + (int)boss->phase) % 3;
        b->attackTimer = 1.5f + i * 0.8f;   // lệch nhau để không bắn cùng lúc
        b->telegraphTimer = 0.0f;
        b->scale = 0.95f;
        b->ringSpawnsRemaining = 0;
        b->ringSpawnTimer = 0.0f;
        b->lastGapAngle = 0.0f;
    }
    boss->boomsRemaining = BOOM_PER_PHASE;
    boss->boomsSpawned = true;
    // orb vàng đầu tiên cho player parry: 3-5s sau khi vào phase
    boss->boomNextOrbDelay = 3.0f + (float)(rand() % 3);
    boss->boomStaggerTimer = 0.0f;
}

static float GetBeamInitialFireTime(Boss *boss, int index) {
    if (boss->boomLaserSkillActive) {
        if (boss->activeBoomSkillType == 1) return 0.6f;
        if (boss->activeBoomSkillType == 2) return 2.0f;
    }
    return BEAM_FIRE_TIME; // 0.45f
}

void BoomTriggerChaoticLasers(Boss *boss) {
    boss->boomLaserSkillActive = true;
    boss->activeBoomSkillType = 1;
    boss->boomSkillTimer = 1.2f + 0.6f; // telegraph + fire
    
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        if (b->state == BOOM_LOOP) {
            beamTelegraph[i] = 1.2f;
            beamFire[i] = 0.0f;
            // Angle range pointing downwards: e.g. 0.2f * PI to 0.8f * PI (approx 36 to 144 degrees)
            float ang = 0.2f * 3.14159f + ((float)rand() / (float)RAND_MAX) * 0.6f * 3.14159f;
            beamDir[i] = (Vector2){ cosf(ang), sinf(ang) };
        } else {
            ClearBoomSkill(i);
        }
    }
}

void BoomTriggerTripleTrackLasers(Boss *boss) {
    boss->boomLaserSkillActive = true;
    boss->activeBoomSkillType = 2;
    boss->boomSkillTimer = 1.2f + 2.0f; // telegraph + fire
    
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        if (b->state == BOOM_LOOP) {
            beamTelegraph[i] = 1.2f;
            beamFire[i] = 0.0f;
            beamDir[i] = (Vector2){ 0.0f, 1.0f }; // pointing straight down initially, will track on update
        } else {
            ClearBoomSkill(i);
        }
    }
}
 
void UpdateBooms(Boss *boss, Vector2 playerPos, OrbManager *om, float dt) {
    (void)om;
    int phase = (int)boss->phase;

    // During intro sequence (0-15s): sequentially spawn booms
    if (boss->state == BOSS_INTRO) {
        float t = boss->introTimer;
        // 0.0s - 5.0s: Left boom BOOM_START, others GONE
        if (t < 5.0f) {
            if (boss->booms[0].state == BOOM_GONE) {
                boss->booms[0].state = BOOM_START;
                boss->booms[0].currentFrame = 0;
                boss->booms[0].animTimer = 0.0f;
            }
            boss->booms[1].state = BOOM_GONE;
            boss->booms[2].state = BOOM_GONE;
        }
        // 5.0s - 10.0s: Right boom BOOM_START, Left boom updates, Center GONE
        else if (t < 10.0f) {
            if (boss->booms[2].state == BOOM_GONE) {
                boss->booms[2].state = BOOM_START;
                boss->booms[2].currentFrame = 0;
                boss->booms[2].animTimer = 0.0f;
            }
            boss->booms[1].state = BOOM_GONE;
        }
        // 10.0s - 15.0s: Center boom BOOM_START, Left and Right update
        else if (t < 15.0f) {
            if (boss->booms[1].state == BOOM_GONE) {
                boss->booms[1].state = BOOM_START;
                boss->booms[1].currentFrame = 0;
                boss->booms[1].animTimer = 0.0f;
            }
        }
    }

    // Check if any major boss skill is active to pause automatic boom node attacks
    bool bossSkillActive = boss->laserActive || boss->slamActive || boss->clawActive || boss->rainActive;
    bool anySkillIsActive = bossSkillActive || boss->boomLaserSkillActive;

    // Pause boom attacks during freeze stage (35s - 41.5s) or early intro (0 - 15s)
    bool isBoomAttackPaused = (boss->state == BOSS_INTRO && (boss->introTimer < 15.0f || (boss->introTimer >= 35.0f && boss->introTimer < 41.5f)));

    if (boss->phaseHitFlash > 0) boss->phaseHitFlash -= dt;

    if (boss->boomLaserSkillActive && !isBoomAttackPaused) {
        boss->boomSkillTimer -= dt;
        if (boss->boomSkillTimer <= 0.0f) {
            boss->boomLaserSkillActive = false;
            for (int k = 0; k < BOOM_PER_PHASE; k++) {
                ClearBoomSkill(k);
            }
        }
    }
 
 
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        if (b->state == BOOM_GONE) { ClearBoomSkill(i); continue; }
 
        b->animTimer += dt;
 
        if (b->state == BOOM_START) {
            if (b->animTimer >= BOOM_FRAME_TIME) {
                b->animTimer = 0.0f;
                b->currentFrame++;
                if (b->currentFrame >= BOOM_START_FRAMES) {
                    b->currentFrame = 0;
                    b->state = BOOM_LOOP;
                }
            }
        } else if (b->state == BOOM_LOOP) {
            // loop animation
            if (b->animTimer >= BOOM_FRAME_TIME) {
                b->animTimer = 0.0f;
                b->currentFrame = (b->currentFrame + 1) % BOOM_LOOP_FRAMES;
            }
 
            if (isBoomAttackPaused) {
                continue;
            }
 
            bool isMiddle = (i == 1);
            // Phase 1: cả 3 cục bắn ring đỏ. Phase >=2: chỉ cục giữa bắn ring đỏ.
            bool shootsRing = (phase == 0) || isMiddle;
 
            // --- Tiến trình laser beam ---
            if (boss->boomLaserSkillActive) {
                if (beamTelegraph[i] > 0.0f) {
                    beamTelegraph[i] -= dt;
                    if (beamTelegraph[i] <= 0.0f) {
                        beamTelegraph[i] = 0.0f;
                        beamFire[i] = (boss->activeBoomSkillType == 1) ? 0.6f : 2.0f;
                    }
                    if (boss->activeBoomSkillType == 2) {
                        // Real-time tracking Y on platform/ground Y
                        Vector2 targetDir = { playerPos.x - b->position.x, playerPos.y - 20.0f - b->position.y };
                        float len = sqrtf(targetDir.x*targetDir.x + targetDir.y*targetDir.y);
                        if (len < 1.0f) { targetDir = (Vector2){0, 1}; len = 1.0f; }
                        targetDir = (Vector2){ targetDir.x / len, targetDir.y / len };
                        float trackSpeed = 4.5f;
                        beamDir[i].x = beamDir[i].x + (targetDir.x - beamDir[i].x) * dt * trackSpeed;
                        beamDir[i].y = beamDir[i].y + (targetDir.y - beamDir[i].y) * dt * trackSpeed;
                        float nlen = sqrtf(beamDir[i].x*beamDir[i].x + beamDir[i].y*beamDir[i].y);
                        if (nlen > 0.001f) { beamDir[i].x /= nlen; beamDir[i].y /= nlen; }
                    }
                } else if (beamFire[i] > 0.0f) {
                    beamFire[i] -= dt;
                    if (beamFire[i] < 0.0f) beamFire[i] = 0.0f;
                }
            } else {
                // --- Tiến trình laser beam tự động (cục trái & phải khi phase>=1) ---
                if (beamTelegraph[i] > 0.0f) {
                    beamTelegraph[i] -= dt;
                    if (beamTelegraph[i] <= 0.0f) {
                        beamTelegraph[i] = 0.0f;
                        beamFire[i] = BEAM_FIRE_TIME;   // hết telegraph -> bắn
                    }
                    if (phase >= 2) {
                        Vector2 targetDir = { playerPos.x - b->position.x, playerPos.y - 20.0f - b->position.y };
                        float len = sqrtf(targetDir.x*targetDir.x + targetDir.y*targetDir.y);
                        if (len < 1.0f) { targetDir = (Vector2){0, 1}; len = 1.0f; }
                        targetDir = (Vector2){ targetDir.x / len, targetDir.y / len };
                        float trackSpeed = 3.0f;
                        beamDir[i].x = beamDir[i].x + (targetDir.x - beamDir[i].x) * dt * trackSpeed;
                        beamDir[i].y = beamDir[i].y + (targetDir.y - beamDir[i].y) * dt * trackSpeed;
                        float nlen = sqrtf(beamDir[i].x*beamDir[i].x + beamDir[i].y*beamDir[i].y);
                        if (nlen > 0.001f) { beamDir[i].x /= nlen; beamDir[i].y /= nlen; }
                    }
                } else if (beamFire[i] > 0.0f) {
                    beamFire[i] -= dt;
                    if (beamFire[i] < 0.0f) beamFire[i] = 0.0f;
                }
            }
 
            // sinh đòn theo cadence tự động
            if (!anySkillIsActive) {
                b->attackTimer -= dt;
                if (b->attackTimer <= 0.0f) {
                    if (shootsRing) {
                        if (phase == 0) {
                            b->ringSpawnsRemaining = 1;
                        } else {
                            b->ringSpawnsRemaining = 5;
                        }
                        b->ringSpawnTimer = 0.0f;
                    } else if (i == 0 || i == 2) {
                        // Tự động bắn laser ở Phase >= 2
                        if (phase >= 1) {
                            Vector2 d = { playerPos.x - b->position.x, playerPos.y - b->position.y };
                            float len = sqrtf(d.x*d.x + d.y*d.y);
                            if (len < 1.0f) { d = (Vector2){0, 1}; len = 1.0f; }
                            beamDir[i] = (Vector2){ d.x / len, d.y / len };
                            beamTelegraph[i] = BEAM_TELEGRAPH_TIME;
                        }
                    }
                    if (shootsRing && phase == 0) {
                        b->attackTimer = 4.5f + (float)(rand() % 150) / 100.0f;
                    } else if (phase == 3) {
                        // Phase 4: outer lasers spawn continuously
                        b->attackTimer = 2.0f + (float)(rand() % 100) / 100.0f; // 2.0s - 3.0s
                    } else {
                        b->attackTimer = 8.0f + (float)(rand() % 400) / 100.0f;
                    }
                }
            }

            // Tiến trình bắn vòng ring liên tiếp
            if (b->ringSpawnsRemaining > 0 && !anySkillIsActive) {
                b->ringSpawnTimer -= dt;
                if (b->ringSpawnTimer <= 0.0f) {
                    int count = 8 + phase * 2;
                    float speed = 150.0f + phase * 25.0f;
                    float gapW = 0.55f - phase * 0.06f; // Phase cao khe hẹp hơn
                    if (gapW < 0.35f) gapW = 0.35f;
                    
                    float g1;
                    int retries = 0;
                    do {
                        g1 = ((float)rand() / (float)RAND_MAX) * 6.2831853f;
                        retries++;
                    } while (fabsf(g1 - b->lastGapAngle) < 1.2f && retries < 10);
                    b->lastGapAngle = g1;
                    
                    FireRing(b->position, count, speed, g1, gapW);
                    
                    b->ringSpawnsRemaining--;
                    b->ringSpawnTimer = 2.2f; // 2.2s delay (greater than 2.0s warning duration) so warnings appear sequentially
                }
            }
        } else if (b->state == BOOM_END) {
            ClearBoomSkill(i);   // đang nổ -> tắt skill còn sót
            if (b->animTimer >= BOOM_FRAME_TIME) {
                b->animTimer = 0.0f;
                b->currentFrame++;
                if (b->currentFrame >= BOOM_END_FRAMES) {
                    b->state = BOOM_GONE;
                }
            }
        }
    }

    // Update ring orbs
    for (int i = 0; i < MAX_RING_ORBS; i++) {
        RingOrb *o = &ringOrbs[i];
        if (!o->active) continue;
        o->life += dt;
        if (o->holdTimer > 0) {
            o->holdTimer -= dt;  // đứng yên telegraph
        } else {
            o->pos.x += o->vel.x * dt;
            o->pos.y += o->vel.y * dt;
        }
        if (o->life > 7.0f || o->pos.x < -40 || o->pos.x > 1320 ||
            o->pos.y < -40 || o->pos.y > 760) {
            o->active = false;
        }
    }
}

int BoomNearestActive(Boss *boss, Vector2 pos) {
    int best = -1;
    float bestD = 1e9f;
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        if (b->state == BOOM_END || b->state == BOOM_GONE) continue;
        float dx = b->position.x - pos.x;
        float dy = b->position.y - pos.y;
        float d = dx*dx + dy*dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void BoomHit(Boss *boss, int index) {
    if (index < 0 || index >= BOOM_PER_PHASE) return;
    BoomNode *b = &boss->booms[index];
    if (b->state == BOOM_END || b->state == BOOM_GONE) return;
    b->state = BOOM_END;
    b->animTimer = 0.0f;
    b->currentFrame = 0;
    ClearBoomSkill(index);
    if (boss->boomsRemaining > 0) boss->boomsRemaining--;
}

// Khoảng cách từ điểm p tới đoạn thẳng a-b.
static float DistPointSeg(Vector2 p, Vector2 a, Vector2 b) {
    float vx = b.x - a.x, vy = b.y - a.y;
    float wx = p.x - a.x, wy = p.y - a.y;
    float c1 = vx*wx + vy*wy;
    float c2 = vx*vx + vy*vy;
    float t = (c2 > 0.0001f) ? (c1 / c2) : 0.0f;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float projx = a.x + t*vx, projy = a.y + t*vy;
    float dx = p.x - projx, dy = p.y - projy;
    return sqrtf(dx*dx + dy*dy);
}

bool CheckPlayerInBoomRings(Boss *boss, Vector2 playerPos) {
    Rectangle pbox = { playerPos.x - 12, playerPos.y - 40, 24, 40 };
    // 1) Ring damage-orb
    for (int i = 0; i < MAX_RING_ORBS; i++) {
        RingOrb *o = &ringOrbs[i];
        if (!o->active || o->holdTimer > 0) continue;  // đang telegraph thì chưa hại
        if (CheckCollisionCircleRec(o->pos, o->radius, pbox)) {
            o->active = false;  // chạm thì tan
            return true;
        }
    }
    // 2) Laser beam (cục trái & phải)
    Vector2 pc = { playerPos.x, playerPos.y - 20.0f }; // tâm thân player
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        if (b->state != BOOM_LOOP) continue;
        // Laser beam đang bắn -> kiểm tra player có nằm trên luồng không.
        if (beamFire[i] > 0.0f) {
            float initTime = GetBeamInitialFireTime(boss, i);
            float elapsed = initTime - beamFire[i];
            float growRatio = elapsed / 0.15f;
            if (growRatio > 1.0f) growRatio = 1.0f;
            float currentLength = GetBeamLength(b->position, beamDir[i]) * growRatio;

            Vector2 end = { b->position.x + beamDir[i].x * currentLength,
                            b->position.y + beamDir[i].y * currentLength };
            if (DistPointSeg(pc, b->position, end) < BEAM_WIDTH * 0.5f + 12.0f) {
                return true;
            }
        }
    }
    return false;
}

static void DrawBoomNode(BoomNode *b) {
    Texture2D tex; int frames;
    if (b->state == BOOM_START)      { tex = boomStartTex; frames = BOOM_START_FRAMES; }
    else if (b->state == BOOM_LOOP)  { tex = boomLoopTex;  frames = BOOM_LOOP_FRAMES; }
    else if (b->state == BOOM_END)   { tex = boomEndTex;   frames = BOOM_END_FRAMES; }
    else return;

    int f = b->currentFrame;
    if (f >= frames) f = frames - 1;

    if (tex.id > 0) {
        Rectangle src = { (float)(f * BOOM_FRAME), 0, (float)BOOM_FRAME, (float)BOOM_FRAME };
        float sz = BOOM_FRAME * b->scale;
        Rectangle dst = { b->position.x, b->position.y, sz, sz };
        Vector2 origin = { sz/2.0f, sz/2.0f };
        DrawTexturePro(tex, src, dst, origin, 0.0f, WHITE);
    } else {
        // fallback: vòng tròn hồng phát sáng
        float pulse = (sinf((float)GetTime()*6.0f)+1.0f)*0.5f;
        DrawCircleV(b->position, 30.0f + pulse*6.0f, (Color){255,80,200,120});
        DrawCircleV(b->position, 16.0f, (Color){255,180,240,255});
    }
}

void DrawBooms(Boss *boss) {
    if (!boomLoaded) return;

    // Vẽ skill 2 cục bên trước (nằm dưới sprite boom).
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        BoomNode *b = &boss->booms[i];
        if (b->state != BOOM_LOOP) continue;

        // --- LASER BEAM (cục trái & phải) ---
        if (beamTelegraph[i] > 0.0f) {
            // Telegraph: vạch mảnh nhấp nháy báo hướng.
            float blink = (sinf((float)GetTime()*22.0f)+1.0f)*0.5f;
            float maxLength = GetBeamLength(b->position, beamDir[i]);
            Vector2 end = { b->position.x + beamDir[i].x * maxLength,
                            b->position.y + beamDir[i].y * maxLength };
            DrawLineEx(b->position, end, 3.0f, (Color){255, 60, 60, (unsigned char)(80 + blink*120)});
        } else if (beamFire[i] > 0.0f) {
            // Firing: Draw Raylib red laser (thick line, fade over time)
            float maxLength = GetBeamLength(b->position, beamDir[i]);
            float initTime = GetBeamInitialFireTime(boss, i);
            float elapsed = initTime - beamFire[i];
            float growRatio = elapsed / 0.15f;
            if (growRatio > 1.0f) growRatio = 1.0f;
            float currentLength = maxLength * growRatio;

            float a = beamFire[i] / initTime;
            Vector2 end = { b->position.x + beamDir[i].x * currentLength,
                            b->position.y + beamDir[i].y * currentLength };
            BeginBlendMode(BLEND_ADDITIVE);
            DrawLineEx(b->position, end, BEAM_WIDTH, (Color){255, 40, 40, (unsigned char)(200*a)});
            DrawLineEx(b->position, end, BEAM_WIDTH*0.5f, (Color){255, 200, 200, (unsigned char)(220*a)});
            EndBlendMode();
        }
    }

    // Vẽ sprite cục boom.
    for (int i = 0; i < BOOM_PER_PHASE; i++) {
        if (boss->booms[i].state != BOOM_GONE) DrawBoomNode(&boss->booms[i]);
    }

    // Ring damage-orbs (đỏ-hồng), telegraph nhấp nháy lúc đứng yên.
    for (int i = 0; i < MAX_RING_ORBS; i++) {
        RingOrb *o = &ringOrbs[i];
        if (!o->active) continue;
        if (ringOrbTex.id > 0) {
            float angle = 0.0f;
            if (o->vel.x != 0.0f || o->vel.y != 0.0f) {
                angle = atan2f(o->vel.y, o->vel.x) * (180.0f / 3.14159265f);
            }
            int frame = (int)(o->life * 12.0f) % 4;
            float scale = 0.3f;
            float size = 128.0f * scale;
            Rectangle source = { (float)(frame * 128), 0.0f, 128.0f, 128.0f };
            Rectangle dest = { o->pos.x, o->pos.y, size, size };
            Vector2 origin = { size / 2.0f, size / 2.0f };
            
            Color tint = WHITE;
            if (o->holdTimer > 0) {
                float blink = (sinf((float)GetTime()*18.0f)+1.0f)*0.5f;
                tint = (Color){255, 100, 200, (unsigned char)(100 + blink*155)};
            }
            DrawTexturePro(ringOrbTex, source, dest, origin, angle, tint);
        } else {
            if (o->holdTimer > 0) {
                float blink = (sinf((float)GetTime()*18.0f)+1.0f)*0.5f;
                DrawCircleV(o->pos, o->radius + 5.0f, (Color){255,60,180,(unsigned char)(60+blink*80)});
                DrawCircleV(o->pos, o->radius, (Color){255,150,230,(unsigned char)(180+blink*70)});
            } else {
                DrawCircleV(o->pos, o->radius + 4.0f, (Color){255,60,180,90});
                DrawCircleV(o->pos, o->radius, (Color){255,120,220,255});
                DrawCircleV(o->pos, o->radius*0.5f, (Color){255,230,255,220});
            }
        }
    }
}