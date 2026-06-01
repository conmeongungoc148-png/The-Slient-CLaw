Total Bytes: 3040
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
#include "cutscene.h"
#include <math.h>

#define BOSS_INTRO_DURATION 15.5f
#define BOSS_ROAR_DURATION 2.0f
#define BOSS_INTRO_ZOOM 1.06f
#define BOSS_ROAR_ZOOM 1.12f
#define BOSS_DEATH_ZOOM 0.94f

static float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float Approach(float current, float target, float speed, float dt) {
    float t = 1.0f - expf(-speed * dt);
    return current + (target - current) * t;
}

void BossCutsceneReset(MyCamera *camera, Vector2 arenaCenter, float screenW, float screenH) {
    *camera = CameraNew(arenaCenter.x, arenaCenter.y, screenW, screenH);
    CameraSetSmoothDamped(camera, 8.0f);
    camera->zoom = BOSS_CUTSCENE_BASE_ZOOM;
    CameraLookAt(camera, arenaCenter);
}

void BossCutsceneUpdate(MyCamera *camera, const Boss *boss, Vector2 arenaCenter, float cameraShake, float dt) {
    Vector2 target = arenaCenter;
    float zoom = BOSS_CUTSCENE_BASE_ZOOM;
    float shake = fabsf(cameraShake);

    if (boss) {
        if (boss->state == BOSS_INTRO) {
            float progress = Clamp01(boss->introTimer / BOSS_INTRO_DURATION);
            target = boss->position;
            zoom = BOSS_CUTSCENE_BASE_ZOOM + (BOSS_INTRO_ZOOM - BOSS_CUTSCENE_BASE_ZOOM) * progress;
            shake += 2.0f + progress * 4.0f;
        } else if (boss->state == BOSS_ROAR) {
            float progress = Clamp01(boss->roarTimer / BOSS_ROAR_DURATION);
            target = boss->position;
            zoom = BOSS_ROAR_ZOOM;
            shake += 8.0f * progress;
        } else if (boss->state == BOSS_DYING || boss->state == BOSS_DEFEATED) {
            float progress = Clamp01(boss->deathTimer / 4.0f);
            target = boss->position;
            zoom = BOSS_DEATH_ZOOM + (BOSS_CUTSCENE_BASE_ZOOM - BOSS_DEATH_ZOOM) * progress;
            shake += 15.0f * (1.0f - progress);
        }
    }

    camera->zoom = Approach(camera->zoom, zoom, 3.0f, dt);
    if (shake > 0.01f) {
        CameraShake(camera, dt * 2.0f, shake);
    }
    CameraUpdate(camera, target, dt);
}

void BossCutsceneDrawOverlay(const Boss *boss, int screenW, int screenH) {
    if (!boss) return;

    if (boss->state == BOSS_INTRO || boss->state == BOSS_ROAR) {
        DrawRectangle(0, 0, screenW, 52, (Color){0, 0, 0, 190});
        DrawRectangle(0, screenH - 52, screenW, 52, (Color){0, 0, 0, 190});
    }

    if (boss->state == BOSS_INTRO) {
        float alpha = Clamp01(boss->introTimer / 3.0f);
        DrawRectangle(0, 0, screenW, screenH, (Color){0, 0, 0, (unsigned char)((1.0f - alpha) * 180)});
    } else if (boss->state == BOSS_ROAR) {
        float pulse = (sinf(boss->roarTimer * 24.0f) + 1.0f) * 0.5f;
        DrawRectangle(0, 0, screenW, screenH, (Color){120, 0, 0, (unsigned char)(pulse * 38)});
    } else if (boss->state == BOSS_DYING) {
        float progress = Clamp01(boss->deathTimer / 4.0f);
        DrawRectangle(0, 0, screenW, screenH, (Color){255, 255, 255, (unsigned char)((1.0f - progress) * boss->deathFlashTimer * 95)});
    }
}
