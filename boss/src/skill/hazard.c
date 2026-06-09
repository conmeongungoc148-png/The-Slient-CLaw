#include "../boss.h"
#include "skill.h"
#include <math.h>
#include <stdlib.h>

void StartHazardAttack(Boss *boss) {
    boss->hazardCount = 8;
    
    for (int i = 0; i < boss->hazardCount; i++) {
        // Random position below boss, spread a bit wider to fit 8 pillars
        boss->hazardPositions[i] = (Vector2){
            boss->position.x + (rand() % 600 - 300),
            BossGetGroundY(boss->position.x)
        };
        // Warning time exact 1.2s
        boss->hazardWarningTime[i] = HAZARD_WARNING_TIME;
        boss->hazardActive[i] = false;
    }
}

void UpdateHazardAttack(Boss *boss, float dt) {
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardWarningTime[i] > 0) {
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] <= 0) {
                boss->hazardActive[i] = true;
            }
        } else if (boss->hazardActive[i]) {
            // Hazard active for 2.0 seconds (80 frames * 25ms)
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] < -2.0f) {
                boss->hazardActive[i] = false;
            }
        }
    }
}

void DrawHazardAttack(Boss *boss, Texture2D hazardTex) {
    for (int i = 0; i < boss->hazardCount; i++) {
        float cx = boss->hazardPositions[i].x;
        float cy = boss->hazardPositions[i].y;
        
        if (boss->hazardWarningTime[i] > 0 && boss->hazardWarningTime[i] <= 1.2f) {
            // --- Telegraph / Warning Zone ---
            float progress = 1.0f - (boss->hazardWarningTime[i] / 1.2f);
            if (progress > 1.0f) progress = 1.0f;
            if (progress < 0.0f) progress = 0.0f;
            
            float scale = 1.2f;
            float shiftY = 7.0f;
            float cy_shifted = cy + shiftY;
            
            // Nhấp nháy liên tục khi sắp nổ (nhanh dần)
            float pulseRate = 15.0f + progress * 20.0f;
            float pulse = (sinf((float)GetTime() * pulseRate) + 1.0f) * 0.5f;
            unsigned char alpha = (unsigned char)(100 + pulse * 155);
            
            // Vẽ vệt đỏ dưới mặt đất báo hiệu nổi bật
            DrawEllipse((int)cx, (int)cy_shifted, (20.0f + progress * 5.0f) * scale, 6.0f * scale, (Color){255, 0, 0, alpha});
            DrawEllipse((int)cx, (int)cy_shifted, 12.0f * scale, 3.0f * scale, (Color){255, 255, 0, alpha});
            
            // Cột báo hiệu đỏ mờ ảo, không vẽ lửa thật
            DrawRectangleGradientV((int)(cx - 15.0f * scale), (int)(cy_shifted - 80.0f * scale), (int)(30.0f * scale), (int)(80.0f * scale), 
                (Color){255, 50, 0, 0}, (Color){255, 0, 0, (unsigned char)(alpha * 0.6f)});
            
        } else if (boss->hazardActive[i]) {
            // --- Actual Fire Pillar Sprite ---
            float elapsed = -boss->hazardWarningTime[i];
            int frame = (int)(elapsed / 0.025f); // 25ms per frame
            if (frame >= 80) frame = 79;
            if (frame < 0) frame = 0;
            
            int col = frame % 10;
            int row = frame / 10;
            
            float scale = 1.2f;
            float shiftY = 7.0f;
            float w = 64.0f * scale;
            float h = 64.0f * scale;
            
            Rectangle sourceRec = { col * 64.0f, row * 64.0f, 64.0f, 64.0f };
            Rectangle destRec = { cx - w / 2.0f, cy - h + shiftY, w, h };
            Vector2 origin = { 0.0f, 0.0f };
            DrawTexturePro(hazardTex, sourceRec, destRec, origin, 0.0f, WHITE);
        }
    }
}
