#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "camera.h"
#include "game.h"

// ─── Fireflies ────────────────────────────────────────────────────────────────
void InitFireflySystem(MyCamera *camera);
void ResetFireflySystem(void);
void UpdateFireflySystem(MyCamera *camera, float dt);
void DrawFireflies(void);

// ─── Rain ─────────────────────────────────────────────────────────────────────
void InitRainSystem(MyCamera *camera);
void ResetRainSystem(void);
void UpdateRainSystem(MyCamera *camera, GameMap *map, float dt);
void DrawMainRain(void);
void DrawForegroundRain(void);

// ─── Overlays ─────────────────────────────────────────────────────────────────
// Warm sunlight gradient from the top (map 0)
void DrawSunlightOverlay(MyCamera *camera, float dt);
// Deep-sapphire twilight tint over the whole viewport
void DrawTwilightOverlay(MyCamera *camera);

#endif // ENVIRONMENT_H
