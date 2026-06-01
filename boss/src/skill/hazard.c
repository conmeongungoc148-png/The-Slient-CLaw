#include "skill.h"
#include <stdlib.h>

void StartHazardAttack(Boss *boss) {
    boss->hazardCount = 3 + rand() % 3;  // 3-5 hazards
    for (int i = 0; i < boss->hazardCount; i++) {
        float hx = 120.0f + (float)(rand() % 1040);
        boss->hazardPositions[i] = (Vector2){
            hx,  // Random X
            BossGetGroundY(hx)  // Dynamically target floor or platform Y
        };
        // Stagger warning times: mỗi hazard cách nhau 0.3s để không cùng lúc
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
    // Original code has no drawing logic for hazards, but we implement this function as declared in skill.h
    (void)boss;
}
