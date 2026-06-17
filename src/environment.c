// environment.c
// All ambient / environmental effects:
//   - Firefly particle system
//   - Rain system (main + foreground)
//   - Sunlight gradient overlay
//   - Twilight colour overlay

#include "environment.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define VIRTUAL_WIDTH  960
#define VIRTUAL_HEIGHT 540

// ═══════════════════════════════════════════════════════════════════════════════
//  FIREFLIES
// ═══════════════════════════════════════════════════════════════════════════════

#define MAX_FIREFLIES 48

typedef struct {
    float x, y;
    float vx, vy;
    float baseSpeed;
    float alpha;
    float size;
    float timer;
    float blinkSpeed;
} Firefly;

static Firefly   fireflies[MAX_FIREFLIES];
static bool      firefliesInitialized = false;

void InitFireflySystem(MyCamera *camera) {
    float camLeft = camera->rl.target.x - VIRTUAL_WIDTH  / 2.0f;
    float camTop  = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;

    for (int i = 0; i < MAX_FIREFLIES; i++) {
        fireflies[i].x          = camLeft + (float)(rand() % VIRTUAL_WIDTH);
        fireflies[i].y          = camTop  + (float)(rand() % VIRTUAL_HEIGHT);
        fireflies[i].baseSpeed  = 10.0f + (float)(rand() % 15);
        fireflies[i].vx         = ((float)(rand() % 100) / 100.0f - 0.5f) * fireflies[i].baseSpeed;
        fireflies[i].vy         = ((float)(rand() % 100) / 100.0f - 0.5f) * fireflies[i].baseSpeed;
        fireflies[i].alpha      = (float)(rand() % 100) / 100.0f;
        fireflies[i].size       = 1.0f + ((float)(rand() % 15) / 10.0f); // 1.0 – 2.5
        fireflies[i].timer      = (float)(rand() % 628) / 100.0f;        // random sine offset
        fireflies[i].blinkSpeed = 0.8f + ((float)(rand() % 15) / 10.0f);
    }
    firefliesInitialized = true;
}

void ResetFireflySystem(void) {
    firefliesInitialized = false;
}

void UpdateFireflySystem(MyCamera *camera, float dt) {
    if (!firefliesInitialized) {
        InitFireflySystem(camera);
        return;
    }

    float camLeft   = camera->rl.target.x - VIRTUAL_WIDTH  / 2.0f;
    float camRight  = camera->rl.target.x + VIRTUAL_WIDTH  / 2.0f;
    float camTop    = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;
    float camBottom = camera->rl.target.y + VIRTUAL_HEIGHT / 2.0f;

    for (int i = 0; i < MAX_FIREFLIES; i++) {
        fireflies[i].timer += dt * fireflies[i].blinkSpeed;
        fireflies[i].alpha  = (sinf(fireflies[i].timer) + 1.0f) * 0.5f;

        fireflies[i].x += (fireflies[i].vx + sinf(fireflies[i].timer) * 8.0f) * dt;
        fireflies[i].y += (fireflies[i].vy + cosf(fireflies[i].timer) * 8.0f) * dt;

        // Wrap around camera bounds
        if (fireflies[i].x < camLeft   - 20.0f) fireflies[i].x = camRight  + 20.0f;
        if (fireflies[i].x > camRight  + 20.0f) fireflies[i].x = camLeft   - 20.0f;
        if (fireflies[i].y < camTop    - 20.0f) fireflies[i].y = camBottom + 20.0f;
        if (fireflies[i].y > camBottom + 20.0f) fireflies[i].y = camTop    - 20.0f;
    }
}

void DrawFireflies(void) {
    if (!firefliesInitialized)
        return;

    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < MAX_FIREFLIES; i++) {
        float         glowRadius = fireflies[i].size * 6.0f;
        unsigned char alphaVal   = (unsigned char)(fireflies[i].alpha * 153); // 60% max opacity

        Color centerColor = (Color){255, 190, 0, alphaVal};
        Color outerColor  = (Color){255, 120, 0, 0};

        DrawCircleGradient((int)fireflies[i].x, (int)fireflies[i].y,
                           glowRadius, centerColor, outerColor);
        DrawCircleV((Vector2){fireflies[i].x, fireflies[i].y},
                    fireflies[i].size * 0.7f,
                    (Color){255, 255, 180, (unsigned char)(fireflies[i].alpha * 255)});
    }
    EndBlendMode();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  RAIN
// ═══════════════════════════════════════════════════════════════════════════════

#define MAX_MAIN_RAIN 180
#define MAX_FG_RAIN    60

typedef struct {
    Vector2 position;
    float   speed;
    float   length;
    bool    isSplashing;
    float   splashTimer;
    Vector2 splashVelocity[3];
    Vector2 splashPosition[3];
} RainDrop;

static RainDrop mainRain[MAX_MAIN_RAIN];
static RainDrop fgRain[MAX_FG_RAIN];
static bool     rainInitialized = false;
static float    rainWindSpeed   = -64.0f; // slight leftward wind

// Helper: find the nearest solid surface below a raindrop's X position
static float GetGroundYForRain(float x, float currentY, GameMap *map) {
    float closestGroundY = (float)map->mapHeight * map->tileHeight;

    for (int i = 0; i < map->layerCount; i++) {
        TMJLayer *layer = &map->layers[i];
        if (!layer->visible || strcmp(layer->type, "objectgroup") != 0)
            continue;

        char lowerName[64];
        strncpy(lowerName, layer->name, 63);
        lowerName[63] = '\0';
        for (int c = 0; lowerName[c]; c++) {
            if (lowerName[c] >= 'A' && lowerName[c] <= 'Z')
                lowerName[c] = lowerName[c] - 'A' + 'a';
        }

        bool isSolid = (strstr(lowerName, "ground")   != NULL) ||
                       (strstr(lowerName, "solid")    != NULL) ||
                       (strstr(lowerName, "soild")    != NULL) ||
                       (strstr(lowerName, "block")    != NULL) ||
                       (strstr(lowerName, "platform") != NULL) ||
                       (strstr(lowerName, "platfrom") != NULL) ||
                       (strstr(lowerName, "water")    != NULL);
        if (!isSolid)
            continue;

        for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (obj->texture.id != 0)
                continue;

            float objX = obj->x + layer->offsetx;
            float objY = obj->y + layer->offsety;

            if (obj->polygonCount == 0) {
                // Rectangle
                if (x >= objX && x <= objX + obj->width) {
                    if (objY >= currentY && objY < closestGroundY)
                        closestGroundY = objY;
                }
            } else {
                // Polygon / slope
                for (int k = 0; k < obj->polygonCount; k++) {
                    Point p1   = obj->polygon[k];
                    Point p2   = obj->polygon[(k + 1) % obj->polygonCount];
                    float x1   = p1.x + objX, y1 = p1.y + objY;
                    float x2   = p2.x + objX, y2 = p2.y + objY;
                    float minX = (x1 < x2) ? x1 : x2;
                    float maxX = (x1 > x2) ? x1 : x2;
                    if (x >= minX && x <= maxX) {
                        float slopeY = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
                        if (slopeY >= currentY && slopeY < closestGroundY)
                            closestGroundY = slopeY;
                    }
                }
            }
        }
    }
    return closestGroundY;
}

void InitRainSystem(MyCamera *camera) {
    float camLeft = camera->rl.target.x - VIRTUAL_WIDTH  / 2.0f;
    float camTop  = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;

    for (int i = 0; i < MAX_MAIN_RAIN; i++) {
        mainRain[i].position.x  = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH  + 200));
        mainRain[i].position.y  = camTop  - 100.0f + (float)(rand() % (VIRTUAL_HEIGHT + 150));
        mainRain[i].speed       = 440.0f + (float)(rand() % 120);
        mainRain[i].length      = 12.0f  + (float)(rand() % 6);
        mainRain[i].isSplashing = false;
        mainRain[i].splashTimer = 0.0f;
    }

    for (int i = 0; i < MAX_FG_RAIN; i++) {
        fgRain[i].position.x  = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH  + 200));
        fgRain[i].position.y  = camTop  - 100.0f + (float)(rand() % (VIRTUAL_HEIGHT + 150));
        fgRain[i].speed       = 560.0f + (float)(rand() % 120);
        fgRain[i].length      = 8.0f   + (float)(rand() % 4);
        fgRain[i].isSplashing = false;
        fgRain[i].splashTimer = 0.0f;
    }
    rainInitialized = true;
}

void ResetRainSystem(void) {
    rainInitialized = false;
}

void UpdateRainSystem(MyCamera *camera, GameMap *map, float dt) {
    if (!rainInitialized) {
        InitRainSystem(camera);
        return;
    }

    float camLeft   = camera->rl.target.x - VIRTUAL_WIDTH  / 2.0f;
    float camRight  = camera->rl.target.x + VIRTUAL_WIDTH  / 2.0f;
    float camTop    = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;
    float camBottom = camera->rl.target.y + VIRTUAL_HEIGHT / 2.0f;

    for (int i = 0; i < MAX_MAIN_RAIN; i++) {
        if (mainRain[i].isSplashing) {
            mainRain[i].splashTimer -= dt;
            for (int k = 0; k < 3; k++) {
                mainRain[i].splashPosition[k].x += mainRain[i].splashVelocity[k].x * dt;
                mainRain[i].splashPosition[k].y += mainRain[i].splashVelocity[k].y * dt;
                mainRain[i].splashVelocity[k].y += 980.0f * dt; // gravity
            }
            if (mainRain[i].splashTimer <= 0.0f) {
                mainRain[i].position.x  = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
                mainRain[i].position.y  = camTop  - 100.0f - (float)(rand() % 50);
                mainRain[i].isSplashing = false;
            }
        } else {
            mainRain[i].position.x += rainWindSpeed * dt;
            mainRain[i].position.y += mainRain[i].speed * dt;

            float groundY = GetGroundYForRain(mainRain[i].position.x,
                                              mainRain[i].position.y, map);
            if (mainRain[i].position.y >= groundY) {
                mainRain[i].isSplashing = true;
                mainRain[i].splashTimer = 0.12f;
                for (int k = 0; k < 3; k++) {
                    mainRain[i].splashPosition[k] = (Vector2){mainRain[i].position.x, groundY};
                    float vx = -80.0f + (k * 80.0f) + (float)(rand() % 40 - 20);
                    float vy = -120.0f - (float)(rand() % 60);
                    mainRain[i].splashVelocity[k] = (Vector2){vx, vy};
                }
            }

            if (mainRain[i].position.y > camBottom + 50.0f  ||
                mainRain[i].position.x < camLeft   - 150.0f ||
                mainRain[i].position.x > camRight  + 150.0f) {
                mainRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
                mainRain[i].position.y = camTop  - 100.0f - (float)(rand() % 50);
            }
        }
    }

    for (int i = 0; i < MAX_FG_RAIN; i++) {
        fgRain[i].position.x += rainWindSpeed * 1.2f * dt;
        fgRain[i].position.y += fgRain[i].speed * dt;

        if (fgRain[i].position.y > camBottom + 50.0f  ||
            fgRain[i].position.x < camLeft   - 150.0f ||
            fgRain[i].position.x > camRight  + 150.0f) {
            fgRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
            fgRain[i].position.y = camTop  - 100.0f - (float)(rand() % 50);
        }
    }
}

void DrawMainRain(void) {
    if (!rainInitialized)
        return;
    for (int i = 0; i < MAX_MAIN_RAIN; i++) {
        if (mainRain[i].isSplashing) {
            for (int k = 0; k < 3; k++) {
                DrawCircle((int)mainRain[i].splashPosition[k].x,
                           (int)mainRain[i].splashPosition[k].y,
                           1.0f, (Color){130, 170, 210, 180});
            }
        } else {
            Vector2 start = mainRain[i].position;
            Vector2 end   = {start.x - rainWindSpeed * (mainRain[i].length / mainRain[i].speed),
                             start.y - mainRain[i].length};
            DrawLineEx(start, end, 1.0f, (Color){130, 170, 210, 110});
        }
    }
}

void DrawForegroundRain(void) {
    if (!rainInitialized)
        return;
    for (int i = 0; i < MAX_FG_RAIN; i++) {
        Vector2 start = fgRain[i].position;
        Vector2 end   = {start.x - (rainWindSpeed * 1.2f) * (fgRain[i].length / fgRain[i].speed),
                         start.y - fgRain[i].length};
        DrawLineEx(start, end, 0.7f, (Color){150, 190, 230, 65});
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  OVERLAYS
// ═══════════════════════════════════════════════════════════════════════════════

static float sunGlowTimer = 0.0f;

void DrawSunlightOverlay(MyCamera *camera, float dt) {
    sunGlowTimer += dt;

    // Slow breathing opacity: 0.05 – 0.09
    float baseOpacity = 0.07f + sinf(sunGlowTimer * 0.5f) * 0.02f;

    float camLeft = camera->rl.target.x - VIRTUAL_WIDTH  / 2.0f;
    float camTop  = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;

    Color topColor    = (Color){255, 225, 130, (unsigned char)(baseOpacity * 255)};
    Color bottomColor = (Color){255, 210,  80, 0};

    DrawRectangleGradientV((int)camLeft - 10, (int)camTop - 10,
                           (int)(VIRTUAL_WIDTH + 20), (int)(VIRTUAL_HEIGHT + 20),
                           topColor, bottomColor);
}

void DrawTwilightOverlay(MyCamera *camera) {
    float camLeft = camera->rl.target.x - VIRTUAL_WIDTH  / 2.0f;
    float camTop  = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;
    // Deep-sapphire blue (~20 % opacity) for the xế chiều atmosphere
    DrawRectangle((int)camLeft - 10, (int)camTop - 10,
                  (int)(VIRTUAL_WIDTH + 20), (int)(VIRTUAL_HEIGHT + 20),
                  (Color){10, 35, 75, 51});
}
