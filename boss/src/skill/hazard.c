#include "../boss.h"
#include <stdlib.h>
#include <math.h>

#define HAZARD_WARNING_TIME 2.5f

void StartHazardAttack(Boss *boss) {
    boss->hazardCount = 3 + rand() % 3;  // 3-5 hazards
    for (int i = 0; i < boss->hazardCount; i++) {
        float hx = 120.0f + (float)(rand() % 1040);
        boss->hazardPositions[i] = (Vector2){
            hx,  // Random X
            BossGetGroundY(hx)  // Dynamically target floor or platform Y
        };
        // Stagger warning times: mỗi hazard cách nhau 0.35s để không cùng lúc
        boss->hazardWarningTime[i] = HAZARD_WARNING_TIME + i * 0.35f;
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
            // Hazard active for 2 seconds then disappear
            boss->hazardWarningTime[i] -= dt;
            if (boss->hazardWarningTime[i] < -2.0f) {
                boss->hazardActive[i] = false;
            }
        }
    }
}

void DrawHazardAttack(Boss *boss) {
    for (int i = 0; i < boss->hazardCount; i++) {
        if (boss->hazardWarningTime[i] > 0) {
            // WARNING PHASE: Flashing red hazard zone warning
            float blink = (sinf(boss->hazardWarningTime[i] * 15.0f) + 1.0f) * 0.5f;
            
            // Draw a flashing warning vertical rectangle
            DrawRectangle((int)(boss->hazardPositions[i].x - 15), (int)(boss->hazardPositions[i].y - 80), 30, 80,
                (Color){255, 50, 0, (unsigned char)(blink * 80)});
            DrawRectangleLines((int)(boss->hazardPositions[i].x - 15), (int)(boss->hazardPositions[i].y - 80), 30, 80,
                (Color){255, 0, 0, (unsigned char)(blink * 200)});
                
            // Draw a warning exclamation mark
            DrawText("!", (int)boss->hazardPositions[i].x - 4, (int)boss->hazardPositions[i].y - 65, 20,
                (Color){255, 255, 0, (unsigned char)(blink * 255)});
        } else if (boss->hazardActive[i]) {
            // ACTIVE PHASE: Draw menacing earth spikes
            // Main center spike
            Vector2 p1 = { boss->hazardPositions[i].x, boss->hazardPositions[i].y - 80 };
            Vector2 p2 = { boss->hazardPositions[i].x - 15, boss->hazardPositions[i].y };
            Vector2 p3 = { boss->hazardPositions[i].x + 15, boss->hazardPositions[i].y };
            DrawTriangle(p1, p2, p3, (Color){140, 50, 50, 255});
            DrawTriangleLines(p1, p2, p3, (Color){230, 90, 90, 255});

            // Side spike left
            Vector2 lp1 = { boss->hazardPositions[i].x - 10, boss->hazardPositions[i].y - 50 };
            Vector2 lp2 = { boss->hazardPositions[i].x - 22, boss->hazardPositions[i].y };
            Vector2 lp3 = { boss->hazardPositions[i].x, boss->hazardPositions[i].y };
            DrawTriangle(lp1, lp2, lp3, (Color){100, 35, 35, 255});
            DrawTriangleLines(lp1, lp2, lp3, (Color){180, 70, 70, 255});

            // Side spike right
            Vector2 rp1 = { boss->hazardPositions[i].x + 10, boss->hazardPositions[i].y - 50 };
            Vector2 rp2 = { boss->hazardPositions[i].x, boss->hazardPositions[i].y };
            Vector2 rp3 = { boss->hazardPositions[i].x + 22, boss->hazardPositions[i].y };
            DrawTriangle(rp1, rp2, rp3, (Color){100, 35, 35, 255});
            DrawTriangleLines(rp1, rp2, rp3, (Color){180, 70, 70, 255});
        }
    }
}
