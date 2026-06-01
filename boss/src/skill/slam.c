#include "skill.h"
#include <math.h>

void StartSlamAttack(Boss *boss, Vector2 playerPos) {
    boss->slamActive = true;
    boss->slamWarningTime = SLAM_WARNING_TIME;  // Warning trước khi đập
    boss->slamTimer = 0.0f;  // Chưa bắt đầu shockwave
    boss->slamPos = (Vector2){playerPos.x, BossGetGroundY(playerPos.x)};  // Vị trí target (player hiện tại)
    boss->shockwaveRadius = 0.0f;
}

void UpdateSlamAttack(Boss *boss, float dt) {
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
}

void DrawSlamAttack(Boss *boss) {
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
}

bool CheckPlayerInShockwave(Vector2 playerPos, Vector2 slamPos, float radius) {
    float dx = playerPos.x - slamPos.x;
    float dy = playerPos.y - slamPos.y;
    float dist = sqrtf(dx*dx + dy*dy);
    // Player bị damage nếu trong vùng shockwave (ring)
    return (dist < radius && dist > radius - 40.0f);
}
