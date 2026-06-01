#include "skill.h"
#include <math.h>
#include <stdlib.h>

void StartClawAttack(Boss *boss, Vector2 playerPos) {
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

void UpdateClawAttack(Boss *boss, float dt) {
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
}

void DrawClawAttack(Boss *boss, Texture2D *splashTexs) {
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
