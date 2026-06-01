#include "../boss.h"
#include <stdlib.h>
#include <math.h>

void StartRainAttack(Boss *boss) {
    boss->rainActive = true;
    boss->rainWarningTime = 2.0f; // 2s warning
    boss->rainDuration = 0.0f;
}

void UpdateRainAttack(Boss *boss, ProjectileManager *pm, float dt) {
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
}

void DrawRainAttack(Boss *boss) {
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
