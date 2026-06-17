#include "skill.h"
#include <math.h>

void StartLaserAttack(Boss *boss, Vector2 playerPos) {
    boss->laserActive = true;
    boss->laserChargeTime = LASER_CHARGE_TIME;
    boss->laserDuration = LASER_DURATION;
    boss->laserStart = boss->position;
    // Project player xuống platform — dù player đang nhảy thì lock vẫn ở mặt sàn
    boss->laserEnd = (Vector2){ playerPos.x, BossGetGroundY(playerPos.x) };
    boss->laserDirection = (Vector2){1, 0};  // Sẽ được lock khi warning kết thúc
}

void UpdateLaserAttack(Boss *boss, Vector2 playerPos, float dt) {
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
                Vector2 groundPlayer = { playerPos.x, BossGetGroundY(playerPos.x) };
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
}

void DrawLaserAttack(Boss *boss) {
    if (!boss->laserActive) return;

    float totalTime = LASER_CHARGE_TIME + LASER_DURATION - boss->laserChargeTime - boss->laserDuration;
    (void)totalTime; // suppress unused warning

    // ── Shared: tính hướng và chiều dài ─────────────────────────────────────
    Vector2 diff = { boss->laserEnd.x - boss->laserStart.x,
                     boss->laserEnd.y - boss->laserStart.y };
    float len = sqrtf(diff.x * diff.x + diff.y * diff.y);
    Vector2 dir = { 0, 1 };
    if (len > 1.0f) { dir.x = diff.x / len; dir.y = diff.y / len; }

    if (boss->laserChargeTime > 0) {
        // ══════════════════════════════════════════════════════════════════
        //  WARNING phase – updated in previous step (kept, no change needed)
        // ══════════════════════════════════════════════════════════════════
        float alpha = (sinf(boss->laserChargeTime * 20.0f) + 1.0f) * 0.5f;
        Color cBand = (Color){255,  30,  30, (unsigned char)(alpha * 140)};
        Color cEdge = (Color){255, 200,   0, (unsigned char)(alpha * 220)};
        Color cDot  = (Color){255,   0,   0, (unsigned char)(alpha * 180)};

        Vector2 farEnd = boss->laserEnd;
        if (len > 1.0f) {
            farEnd.x = boss->laserEnd.x + dir.x * 1300.0f;
            farEnd.y = boss->laserEnd.y + dir.y * 1300.0f;
        }
        DrawLineEx(boss->laserStart, farEnd, 20.0f, cBand);
        DrawLineEx(boss->laserStart, farEnd,  2.0f, cEdge);
        DrawCircleV(boss->laserEnd, 8.0f, cDot);

    } else {
        // ══════════════════════════════════════════════════════════════════
        //  FIRING phase – Evil Mage Laser (fully procedural, red/purple)
        // ══════════════════════════════════════════════════════════════════

        // Fade out in the last 0.5 s
        float alpha = 1.0f;
        if (boss->laserDuration < 0.5f) alpha = boss->laserDuration / 0.5f;
        float elapsed = LASER_DURATION - boss->laserDuration;

        // Pulsating width (core throbs like dark energy)
        float pulse   = sinf(elapsed * 18.0f);          // fast throb
        float pulse2  = sinf(elapsed * 7.0f + 1.2f);   // slow undulation

        // ── 1. Outer corona – wide, soft dark-purple ─────────────────────
        float outerW = 28.0f + pulse2 * 4.0f;
        DrawLineEx(boss->laserStart, boss->laserEnd, outerW,
                   (Color){90, 0, 160, (unsigned char)(alpha * 55)});

        // ── 2. Mid glow – crimson-purple ─────────────────────────────────
        float midW = 16.0f + pulse * 2.5f;
        DrawLineEx(boss->laserStart, boss->laserEnd, midW,
                   (Color){180, 20, 120, (unsigned char)(alpha * 130)});

        // ── 3. Core beam – bright red-magenta ────────────────────────────
        float coreW = 8.0f + pulse * 1.5f;
        DrawLineEx(boss->laserStart, boss->laserEnd, coreW,
                   (Color){255, 60,  80, (unsigned char)(alpha * 200)});

        // ── 4. Hot centre – near-white pink ──────────────────────────────
        float hotW = 3.0f + (pulse + 1.0f) * 0.5f;
        DrawLineEx(boss->laserStart, boss->laserEnd, hotW,
                   (Color){255, 200, 230, (unsigned char)(alpha * 240)});

        // ── 5. Origin blast circle ────────────────────────────────────────
        float blastR = 14.0f + pulse2 * 3.0f;
        DrawCircleV(boss->laserStart, blastR,
                    (Color){255, 100, 200, (unsigned char)(alpha * 180)});
        DrawCircleV(boss->laserStart, blastR * 0.5f,
                    (Color){255, 240, 255, (unsigned char)(alpha * 230)});

        // ── 6. Particle sparks along the beam ────────────────────────────
        // Use a deterministic pseudo-random pattern seeded by elapsed so
        // sparks appear to flicker without needing a particle array.
        BeginBlendMode(BLEND_ADDITIVE);
        int sparkCount = 18;
        for (int i = 0; i < sparkCount; i++) {
            // Distribute sparks along beam length
            float t  = (float)i / (float)(sparkCount - 1);
            float jx = sinf(elapsed * 13.7f + i * 2.3f) * 7.0f;
            float jy = cosf(elapsed * 11.1f + i * 1.7f) * 7.0f;
            // Perpendicular offset
            Vector2 perp = { -dir.y, dir.x };
            float poff = sinf(elapsed * 9.3f + i * 3.1f) * 9.0f;

            Vector2 sp = {
                boss->laserStart.x + dir.x * len * t + perp.x * poff + jx,
                boss->laserStart.y + dir.y * len * t + perp.y * poff + jy
            };
            float sparkAlpha = alpha * (0.5f + 0.5f * sinf(elapsed * 15.0f + i * 1.9f));
            float sparkR = 2.5f + sinf(elapsed * 8.0f + i) * 1.2f;
            // Alternate spark colour between red-orange and purple
            Color sparkCol = (i % 2 == 0)
                ? (Color){255, 80,  30, (unsigned char)(sparkAlpha * 200)}
                : (Color){180, 30, 255, (unsigned char)(sparkAlpha * 200)};
            DrawCircleV(sp, sparkR, sparkCol);
        }
        EndBlendMode();

        // ── 7. Tip flare at laserEnd ─────────────────────────────────────
        float tipR = 10.0f + pulse * 3.0f;
        DrawCircleV(boss->laserEnd, tipR,
                    (Color){255, 80, 160, (unsigned char)(alpha * 160)});
        DrawCircleV(boss->laserEnd, tipR * 0.4f,
                    (Color){255, 230, 255, (unsigned char)(alpha * 220)});
    }
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
