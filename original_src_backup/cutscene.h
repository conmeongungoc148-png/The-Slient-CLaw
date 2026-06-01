Total Bytes: 422
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
#ifndef CUTSCENE_H
#define CUTSCENE_H

#include "boss.h"
#include "camera.h"
#include "raylib.h"

#define BOSS_CUTSCENE_BASE_ZOOM 1.0f

void BossCutsceneReset(MyCamera *camera, Vector2 arenaCenter, float screenW, float screenH);
void BossCutsceneUpdate(MyCamera *camera, const Boss *boss, Vector2 arenaCenter, float cameraShake, float dt);
void BossCutsceneDrawOverlay(const Boss *boss, int screenW, int screenH);

#endif
