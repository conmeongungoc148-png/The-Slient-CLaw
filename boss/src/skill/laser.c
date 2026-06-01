#include "../boss.h"
#include <math.h>

#define LASER_CHARGE_TIME 2.0f   // 2s cảnh báo nhấp nháy
#define LASER_DURATION 2.0f      // Max duration (thường tắt sớm khi chạm mép)
#define LASER_EXTEND_SPEED 300.0f // Tốc độ laser kéo dài từ lockPos theo direction

#define LASER_FRAMES 8
#define LASER_COLS 4
#define LASER_FW 300
#define LASER_FH 1309

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
        // Gốc luôn là vị trí boss
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
    if (boss->laserActive) {
        if (boss->laserChargeTime > 0) {
            // Charging: thin red line blinking
            float alpha = (sinf(boss->laserChargeTime * 20.0f) + 1.0f) * 0.5f;
            Color c = (Color){255, 0, 0, (unsigned char)(alpha * 150)};
            DrawLineEx(boss->laserStart, boss->laserEnd, 3.0f, c);
            // Warning circle at end
            DrawCircleV(boss->laserEnd, 20.0f, (Color){255, 0, 0, (unsigned char)(alpha * 100)});
        } else {
            Texture2D laserTex = GetBossLaserTex();
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
