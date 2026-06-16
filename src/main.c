#include "camera.h"
#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "boss.h"
#include "boss_player.h"
#include "collision.h"
#include "gamestate.h"
#include "map.h"
#include "orb.h"
#include "projectile.h"

// === Boss Fight State & Assets ===
static bool bossInitialized = false;
static BossPlayer bossPlayer;
static Boss boss;
static ProjectileManager pm;
static OrbManager om;
static cute_tiled_map_t *cuteBossMap = NULL;
static GameState bossGameState = STATE_PLAYING;

static Texture2D texAgis;

static bool prevClawActive = false;
static bool prevLaserActive = false;
static int prevHazardCount = 0;
static bool clawSlashPlayed = false;
static bool prevRainActive = false;

extern int gPreIntroSlowWalk;
static Vector2 bossTargetPos = {608.0f, 220.0f};

static Vector2 FindBossPosition(GameMap *map, Vector2 fallback) {
  for (int i = 0; i < map->layerCount; i++) {
    TMJLayer *layer = &map->layers[i];
    if (strcmp(layer->name, "boss") == 0 || strcmp(layer->name, "agis") == 0) {
      for (int j = 0; j < layer->objectCount; j++) {
        TMJObject *obj = &layer->objects[j];
        if (obj->width > 100.0f) {
          float centerX = obj->x + layer->offsetx + obj->width / 2.0f;
          float centerY = obj->y + layer->offsety - obj->height / 2.0f;
          TraceLog(
              LOG_INFO,
              "[BOSS] Found boss object in layer '%s' at center: %.2f, %.2f",
              layer->name, centerX, centerY);
          return (Vector2){centerX, centerY};
        }
      }
    }
  }
  return fallback;
}

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define VIRTUAL_HEIGHT 540
#define VIRTUAL_WIDTH 960

Vector2 FindSpawnPoint(GameMap *map, Vector2 defaultPos) {
  for (int i = 0; i < map->layerCount; i++) {
    if (strcmp(map->layers[i].type, "objectgroup") == 0) {
      for (int j = 0; j < map->layers[i].objectCount; j++) {
        TMJObject *obj = &map->layers[i].objects[j];
        if (TextIsEqual(obj->name, "spawn") ||
            TextIsEqual(obj->name, "Spawn") ||
            TextIsEqual(map->layers[i].name, "spawn") ||
            TextIsEqual(map->layers[i].name, "Spawn")) {
          float spawnX = obj->x + map->layers[i].offsetx;
          float spawnY = obj->y + map->layers[i].offsety + obj->height - 64.0f;
          TraceLog(LOG_INFO, "[SPAWN] Found spawn point at: %.2f, %.2f", spawnX,
                   spawnY);
          return (Vector2){spawnX, spawnY};
        }
      }
    }
  }
  TraceLog(LOG_WARNING,
           "[SPAWN] Spawn point NOT found! Using default: %.2f, %.2f",
           defaultPos.x, defaultPos.y);
  return defaultPos;
}

void GetMapBackgroundBounds(GameMap *map, float *minX, float *maxX, float *minY,
                            float *maxY) {
  *minX = 0.0f;
  *maxX = (float)map->mapWidth * map->tileWidth;
  *minY = 0.0f;
  *maxY = (float)map->mapHeight * map->tileHeight;

  bool foundBackground = false;
  float bgMinX = 100000.0f;
  float bgMaxX = -100000.0f;
  float bgMinY = 100000.0f;
  float bgMaxY = -100000.0f;

  for (int i = 0; i < map->layerCount; i++) {
    TMJLayer *layer = &map->layers[i];
    if (!layer->visible)
      continue;

    char lowerName[64];
    strncpy(lowerName, layer->name, 63);
    lowerName[63] = '\0';
    for (int c = 0; lowerName[c]; c++) {
      if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') {
        lowerName[c] = lowerName[c] - 'A' + 'a';
      }
    }

    if (strstr(lowerName, "background") != NULL) {
      if (strcmp(layer->type, "objectgroup") == 0) {
        for (int j = 0; j < layer->objectCount; j++) {
          TMJObject *obj = &layer->objects[j];
          float objX = obj->x + layer->offsetx;
          float objY = obj->y + layer->offsety;

          float top, bottom, left, right;
          if (obj->texture.id != 0) {
            top = objY - obj->height;
            bottom = objY;
          } else {
            top = objY;
            bottom = objY + obj->height;
          }
          left = objX;
          right = objX + obj->width;

          if (left < bgMinX)
            bgMinX = left;
          if (right > bgMaxX)
            bgMaxX = right;
          if (top < bgMinY)
            bgMinY = top;
          if (bottom > bgMaxY)
            bgMaxY = bottom;
          foundBackground = true;
        }
      }
    }
  }

  if (foundBackground) {
    if (bgMinX < 0.0f)
      bgMinX = 0.0f;
    if (bgMaxX > *maxX)
      bgMaxX = *maxX;
    if (bgMinY < 0.0f)
      bgMinY = 0.0f;
    if (bgMaxY > *maxY)
      bgMaxY = *maxY;

    *minX = bgMinX;
    *maxX = bgMaxX;
    *minY = bgMinY;
    *maxY = bgMaxY;
  }
}

Font gGameFont;

#define MAX_MAIN_RAIN 180
#define MAX_FG_RAIN 60

typedef struct {
    Vector2 position;
    float speed;
    float length;
    bool isSplashing;
    float splashTimer;
    Vector2 splashVelocity[3];
    Vector2 splashPosition[3];
} RainDrop;

static RainDrop mainRain[MAX_MAIN_RAIN];
static RainDrop fgRain[MAX_FG_RAIN];
static bool rainInitialized = false;

float GetGroundYForRain(float x, float currentY, GameMap *map) {
    float closestGroundY = (float)map->mapHeight * map->tileHeight; // Fallback is bottom of the map
    
    for (int i = 0; i < map->layerCount; i++) {
        TMJLayer *layer = &map->layers[i];
        if (!layer->visible || strcmp(layer->type, "objectgroup") != 0)
            continue;
            
        // Check if it is a ground or platform layer
        char lowerName[64];
        strncpy(lowerName, layer->name, 63);
        lowerName[63] = '\0';
        for (int c = 0; lowerName[c]; c++) {
            if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') {
                lowerName[c] = lowerName[c] - 'A' + 'a';
            }
        }
        
        bool isSolid = (strstr(lowerName, "ground") != NULL) ||
                       (strstr(lowerName, "solid") != NULL) ||
                       (strstr(lowerName, "soild") != NULL) ||
                       (strstr(lowerName, "block") != NULL) ||
                       (strstr(lowerName, "platform") != NULL) ||
                       (strstr(lowerName, "platfrom") != NULL);
                       
        if (!isSolid)
            continue;
            
        for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (obj->texture.id != 0)
                continue;
                
            float objX = obj->x + layer->offsetx;
            float objY = obj->y + layer->offsety;
            float objWidth = obj->width;
            
            // If it's a rectangle
            if (obj->polygonCount == 0) {
                if (x >= objX && x <= objX + objWidth) {
                    if (objY >= currentY && objY < closestGroundY) {
                        closestGroundY = objY;
                    }
                }
            } else {
                // Polygon/Slope (slopeY check)
                for (int k = 0; k < obj->polygonCount; k++) {
                    Point p1 = obj->polygon[k];
                    Point p2 = obj->polygon[(k + 1) % obj->polygonCount];
                    float x1 = p1.x + objX;
                    float y1 = p1.y + objY;
                    float x2 = p2.x + objX;
                    float y2 = p2.y + objY;
                    float minX = (x1 < x2) ? x1 : x2;
                    float maxX = (x1 > x2) ? x1 : x2;
                    if (x >= minX && x <= maxX) {
                        float slopeY = y1 + (y2 - y1) * (x - x1) / (x2 - x1);
                        if (slopeY >= currentY && slopeY < closestGroundY) {
                            closestGroundY = slopeY;
                        }
                    }
                }
            }
        }
    }
    return closestGroundY;
}

void InitRainSystem(MyCamera *camera) {
    float camLeft = camera->rl.target.x - VIRTUAL_WIDTH / 2.0f;
    float camTop = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;
    
    for (int i = 0; i < MAX_MAIN_RAIN; i++) {
        mainRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
        mainRain[i].position.y = camTop - 100.0f + (float)(rand() % (VIRTUAL_HEIGHT + 150));
        mainRain[i].speed = 550.0f + (float)(rand() % 150);
        mainRain[i].length = 12.0f + (float)(rand() % 6);
        mainRain[i].isSplashing = false;
        mainRain[i].splashTimer = 0.0f;
    }
    
    for (int i = 0; i < MAX_FG_RAIN; i++) {
        fgRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
        fgRain[i].position.y = camTop - 100.0f + (float)(rand() % (VIRTUAL_HEIGHT + 150));
        fgRain[i].speed = 700.0f + (float)(rand() % 150);
        fgRain[i].length = 8.0f + (float)(rand() % 4);
        fgRain[i].isSplashing = false;
        fgRain[i].splashTimer = 0.0f;
    }
    rainInitialized = true;
}

void UpdateRainSystem(MyCamera *camera, GameMap *map, float dt) {
    if (!rainInitialized) {
        InitRainSystem(camera);
        return;
    }
    
    float camLeft = camera->rl.target.x - VIRTUAL_WIDTH / 2.0f;
    float camRight = camera->rl.target.x + VIRTUAL_WIDTH / 2.0f;
    float camTop = camera->rl.target.y - VIRTUAL_HEIGHT / 2.0f;
    float camBottom = camera->rl.target.y + VIRTUAL_HEIGHT / 2.0f;
    
    float windSpeed = -80.0f; // slight wind blowing to the left
    
    for (int i = 0; i < MAX_MAIN_RAIN; i++) {
        if (mainRain[i].isSplashing) {
            mainRain[i].splashTimer -= dt;
            for (int k = 0; k < 3; k++) {
                mainRain[i].splashPosition[k].x += mainRain[i].splashVelocity[k].x * dt;
                mainRain[i].splashPosition[k].y += mainRain[i].splashVelocity[k].y * dt;
                mainRain[i].splashVelocity[k].y += 980.0f * dt; // gravity
            }
            if (mainRain[i].splashTimer <= 0.0f) {
                mainRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
                mainRain[i].position.y = camTop - 100.0f - (float)(rand() % 50);
                mainRain[i].isSplashing = false;
            }
        } else {
            mainRain[i].position.x += windSpeed * dt;
            mainRain[i].position.y += mainRain[i].speed * dt;
            
            float groundY = GetGroundYForRain(mainRain[i].position.x, mainRain[i].position.y, map);
            
            if (mainRain[i].position.y >= groundY) {
                mainRain[i].isSplashing = true;
                mainRain[i].splashTimer = 0.12f;
                for (int k = 0; k < 3; k++) {
                    mainRain[i].splashPosition[k] = (Vector2){ mainRain[i].position.x, groundY };
                    float vx = -80.0f + (k * 80.0f) + (float)(rand() % 40 - 20);
                    float vy = -120.0f - (float)(rand() % 60);
                    mainRain[i].splashVelocity[k] = (Vector2){ vx, vy };
                }
            }
            
            if (mainRain[i].position.y > camBottom + 50.0f || 
                mainRain[i].position.x < camLeft - 150.0f || 
                mainRain[i].position.x > camRight + 150.0f) {
                mainRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
                mainRain[i].position.y = camTop - 100.0f - (float)(rand() % 50);
            }
        }
    }
    
    for (int i = 0; i < MAX_FG_RAIN; i++) {
        fgRain[i].position.x += windSpeed * 1.2f * dt;
        fgRain[i].position.y += fgRain[i].speed * dt;
        
        if (fgRain[i].position.y > camBottom + 50.0f || 
            fgRain[i].position.x < camLeft - 150.0f || 
            fgRain[i].position.x > camRight + 150.0f) {
            fgRain[i].position.x = camLeft - 100.0f + (float)(rand() % (VIRTUAL_WIDTH + 200));
            fgRain[i].position.y = camTop - 100.0f - (float)(rand() % 50);
        }
    }
}

void DrawMainRain(void) {
    for (int i = 0; i < MAX_MAIN_RAIN; i++) {
        if (mainRain[i].isSplashing) {
            for (int k = 0; k < 3; k++) {
                DrawCircle(mainRain[i].splashPosition[k].x, mainRain[i].splashPosition[k].y, 1.0f, (Color){ 130, 170, 210, 180 });
            }
        } else {
            Vector2 start = mainRain[i].position;
            Vector2 end = {
                start.x - 80.0f * (mainRain[i].length / 550.0f),
                start.y - mainRain[i].length
            };
            DrawLineEx(start, end, 1.0f, (Color){ 130, 170, 210, 110 });
        }
    }
}

void DrawForegroundRain(void) {
    for (int i = 0; i < MAX_FG_RAIN; i++) {
        Vector2 start = fgRain[i].position;
        Vector2 end = {
            start.x - 80.0f * 1.2f * (fgRain[i].length / 700.0f),
            start.y - fgRain[i].length
        };
        DrawLineEx(start, end, 0.7f, (Color){ 150, 190, 230, 65 });
    }
}

static BossPlayer outroFakeCat;
static bool outroFakeCatActive = false;
static float outroFakeCatAlpha = 1.0f;
static float outroFlashTimer = 0.0f;

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
             "The Forest - Full Screen Infinite Map");
  gGameFont = LoadFontEx("assets/other/MedievalSharp-Regular.ttf", 96, NULL, 0);
  SetTextureFilter(gGameFont.texture, TEXTURE_FILTER_BILINEAR);
  SetTargetFPS(60);

  Audio_InitDevice();

  RenderTexture2D target = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

  Shader grayscaleShader = LoadShader(0, "assets/grayscale.fs");
  Shader shockwaveShader = LoadShader(0, "assets/shockwave.fs");
  int swCenterLoc = GetShaderLocation(shockwaveShader, "center");
  int swTimeLoc = GetShaderLocation(shockwaveShader, "time");
  int swParamsLoc = GetShaderLocation(shockwaveShader, "shockParams");

  const char *mapList[] = {"assets/thesecondmap.tmj",
                           "boss/assets/boss map 1v1.tmj"};
  int totalMaps = 2;
  int currentMapIndex = 0;

  GameMap gameMap = LoadMapData(mapList[currentMapIndex]);

  // Find spawn point from the map, or fallback to default
  Vector2 startPos = FindSpawnPoint(&gameMap, (Vector2){50.0f, 1168.0f});

  Player player = {0};
  InitPlayer(&player, startPos);

  Texture2D texIdle = LoadTexture("assets/cat/IDLE.png");
  Texture2D texWalk = LoadTexture("assets/cat/WALK.png");
  Texture2D texRun = LoadTexture("assets/cat/RUN.png");
  Texture2D texJump = LoadTexture("assets/cat/JUMP.png");
  Texture2D texAttack = LoadTexture("assets/cat/ATTACK 1.png");
  Texture2D texRunJump = LoadTexture("assets/cat/RUNNING JUMP.png");
  Texture2D texHurt = LoadTexture("assets/cat/HURT.png");

  MyCamera myCam = CameraNew(player.position.x, player.position.y,
                             VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  myCam.zoom = 1.9f;
  CameraSetSmoothDamped(&myCam, 10.0f);
  float bgMinX, bgMaxX, bgMinY, bgMaxY;
  GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
  CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.1f)
      dt = 0.016f; // Cap dt to prevent physics glitches when loading a map
                   // takes time!

    // --- UPDATE ---
    if (currentMapIndex == 1) {
      if (bossInitialized) {
        Audio_Update(dt, &boss);
      }

      // === BOSS FIGHT LOGIC UPDATE ===
      if (bossGameState == STATE_PLAYING) {
        // Skip phase cheat
        if (IsKeyPressed(KEY_K)) {
          if (boss.state == BOSS_FIGHTING) {
            boss.boomsRemaining = 0;
            TraceLog(LOG_INFO, "CHEAT: Skipped fight phase!");
          } else if (boss.state == BOSS_OUTRO) {
            boss.outroTimer = 999.0f; // skip outro cutscene
            TraceLog(LOG_INFO, "CHEAT: Skipped outro cutscene!");
          }
        }

        // Đi-bộ-only: pre-intro
        gPreIntroSlowWalk = (boss.state == BOSS_PRE_INTRO);
        static float activeCameraShake = 0.0f;
        float bossCameraShake = 0.0f;
        UpdateBoss(&boss, bossPlayer.position, &pm, &om, dt, &bossCameraShake);

        bool bossSetsShakeEveryFrame =
            (boss.state == BOSS_INTRO) || (boss.state == BOSS_ROAR) ||
            (boss.state == BOSS_OUTRO) || (boss.state == BOSS_DYING);
        if (bossSetsShakeEveryFrame) {
          activeCameraShake = bossCameraShake;
        } else {
          if (bossCameraShake > 0.0f) {
            if (bossCameraShake > activeCameraShake) {
              activeCameraShake = bossCameraShake;
            }
          } else {
            activeCameraShake -= dt * 40.0f;
            if (activeCameraShake < 0.0f)
              activeCameraShake = 0.0f;
          }
        }

        if (activeCameraShake > 0.0f) {
          CameraShake(&myCam, 0.05f, activeCameraShake);
        }

        // Player update logic (with lock check)
        bool isPlayerLocked =
            (boss.state == BOSS_PRE_INTRO && boss.preIntroTriggered) ||
            (boss.state == BOSS_ROAR);
        if (boss.state == BOSS_INTRO &&
            (boss.introTimer < 15.0f || boss.introTimer >= 35.0f))
          isPlayerLocked = true;
        if (boss.state == BOSS_OUTRO)
          isPlayerLocked = true;

        if (isPlayerLocked) {
          bossPlayer.velocity = (Vector2){0, 0};
          bossPlayer.state = PSTATE_IDLE;
          bossPlayer.isJumping = false;
          bossPlayer.isRunning = false;
          bossPlayer.isSprinting = false;
          bossPlayer.isAttacking = false;
          bossPlayer.isHurt = false;
          if (boss.state == BOSS_OUTRO) {
            bossPlayer.position.x = 550.0f;
            bossPlayer.facingRight = true;
          } else {
            bossPlayer.position.x = boss.beaconPos.x;
          }
          bossPlayer.position.y =
              419.0f; // Force player to ground level during cinematic lock
          bossPlayer.frameTimer += dt;
          if (bossPlayer.frameTimer >= 0.12f) {
            bossPlayer.frameTimer = 0.0f;
            bossPlayer.currentFrame = (bossPlayer.currentFrame + 1) % 10;
          }
          bossPlayer.hurtBox.x = bossPlayer.position.x - 10;
          bossPlayer.hurtBox.y = bossPlayer.position.y - 30;
        } else {
          if (boss.state == BOSS_PRE_INTRO || boss.state == BOSS_INTRO ||
              boss.state == BOSS_FIGHTING || boss.state == BOSS_OUTRO) {
            UpdateBossPlayerOnMap(&bossPlayer, cuteBossMap, 0.0f, 419.0f, dt);
            bossPlayer.hurtBox.x = bossPlayer.position.x - 10;
            bossPlayer.hurtBox.y = bossPlayer.position.y - 30;
          }
        }

        if (boss.state == BOSS_FIGHTING) {
          UpdateProjectiles(&pm, dt);
          UpdateOrbs(&om, bossPlayer.position, boss.position, 419.0f, dt);

          // Audio Alarm, Claw, etc.
          if (boss.clawActive && !prevClawActive) {
            Audio_PlaySFX(SFX_ALARM);
          }
          if (boss.clawActive && boss.clawWarningTime <= 0 &&
              boss.clawDuration > 0 && !clawSlashPlayed) {
            Audio_StopSFX(SFX_ALARM);
            Audio_PlaySFX(SFX_SLASH);
            clawSlashPlayed = true;
          }
          if (!boss.clawActive)
            clawSlashPlayed = false;
          prevClawActive = boss.clawActive;

          if (boss.laserActive && !prevLaserActive) {
            Audio_PlaySFX(SFX_ALARM);
          }
          if (prevLaserActive && boss.laserActive &&
              boss.laserChargeTime <= 0) {
            Audio_StopSFX(SFX_ALARM);
          }
          prevLaserActive = boss.laserActive;

          if (boss.hazardCount > 0 && prevHazardCount == 0) {
            Audio_PlaySFX(SFX_ALARM);
          }
          prevHazardCount = boss.hazardCount;
          bool anyHazardActive = false;
          for (int i = 0; i < boss.hazardCount; i++) {
            if (boss.hazardActive[i] || boss.hazardWarningTime[i] > 0) {
              anyHazardActive = true;
              break;
            }
          }
          if (!anyHazardActive && prevHazardCount > 0) {
            Audio_StopSFX(SFX_ALARM);
            prevHazardCount = 0;
          }

          if (boss.rainActive && !prevRainActive) {
            Audio_PlaySFX(SFX_ALARM);
          }
          if (prevRainActive && boss.rainActive && boss.rainWarningTime <= 0) {
            Audio_StopSFX(SFX_ALARM);
          }
          prevRainActive = boss.rainActive;

          // Orb catch (Player collects orb on floor and it flies to boom node
          // or boss)
          {
            bool wasReady[MAX_ORBS];
            for (int i = 0; i < MAX_ORBS; i++) {
              wasReady[i] = (om.orbs[i].state == ORB_READY);
            }
            Vector2 orbTarget = boss.position;
            int bi = BoomNearestActive(&boss, bossPlayer.position);
            if (bi >= 0) {
              orbTarget = boss.booms[bi].position;
            }
            TryCatchOrb(&om, bossPlayer.hurtBox, orbTarget);
            for (int i = 0; i < MAX_ORBS; i++) {
              if (wasReady[i] && om.orbs[i].state == ORB_RETURNING) {
                Audio_PlaySFX(SFX_HITS);
              }
            }
          }

          // Projectile damage to player
          for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!pm.projectiles[i].active)
              continue;
            if (CheckCollision(pm.projectiles[i].hitbox, bossPlayer.hurtBox)) {
              BossPlayerTakeDamage(&bossPlayer, pm.projectiles[i].damage);
              pm.projectiles[i].active = false;
              pm.count--;
            }
          }

          for (int i = 0; i < MAX_ORBS; i++) {
            if (om.orbs[i].state != ORB_RETURNING)
              continue;
            int bi = BoomNearestActive(&boss, om.orbs[i].position);
            if (bi >= 0) {
              float dx = boss.booms[bi].position.x - om.orbs[i].position.x;
              float dy = boss.booms[bi].position.y - om.orbs[i].position.y;
              if (dx * dx + dy * dy < 70.0f * 70.0f) {
                BoomHit(&boss, bi);
                Audio_PlaySFX(SFX_DAMAGE);
                om.orbs[i].state = ORB_INACTIVE;
              }
            }
          }

          // Ring damage-orb (từ cục boom) chạm player -> mất máu
          if (CheckPlayerInBoomRings(&boss, bossPlayer.position)) {
            BossPlayerTakeDamage(&bossPlayer, 1);
          }

          // Other hazards damage
          if (boss.laserActive && boss.laserChargeTime <= 0) {
            if (CheckPlayerInLaser(bossPlayer.position, boss.laserStart,
                                   boss.laserEnd, 20.0f)) {
              BossPlayerTakeDamage(&bossPlayer, 1);
            }
          }
          if (boss.slamActive && boss.shockwaveRadius > 50.0f) {
            if (CheckPlayerInShockwave(bossPlayer.position, boss.slamPos,
                                       boss.shockwaveRadius)) {
              BossPlayerTakeDamage(&bossPlayer, 2);
            }
          }
          if (boss.clawActive && boss.clawWarningTime <= 0 &&
              boss.clawDuration > 0) {
            if (CheckPlayerInClawZone(bossPlayer.position, boss.clawZone)) {
              BossPlayerTakeDamage(&bossPlayer, 2);
            }
          }
          for (int i = 0; i < boss.hazardCount; i++) {
            if (!boss.hazardActive[i])
              continue;
            Rectangle hazardRect = {boss.hazardPositions[i].x - 10.8f,
                                    boss.hazardPositions[i].y - 48.2f, 21.6f,
                                    53.52f};
            if (CheckCollision(hazardRect, bossPlayer.hurtBox)) {
              BossPlayerTakeDamage(&bossPlayer, 1);
            }
          }
        } else {
          Audio_StopSFX(SFX_ALARM);
        }

        if (boss.state == BOSS_OUTRO) {
          float t = boss.outroTimer;
          if (t < 5.0f) {
            boss.outroRedOrbActive = true;
            boss.outroRedOrbPos.x = boss.position.x;
            boss.outroRedOrbPos.y =
                boss.position.y - 120.0f - (t / 5.0f) * 40.0f;
            boss.outroRedOrbRadius = (t / 5.0f) * 100.0f;
          } else if (t < 10.0f) {
            boss.outroRedOrbActive = true;
            boss.outroRedOrbPos.x = boss.position.x;
            boss.outroRedOrbPos.y = boss.position.y - 160.0f;
            boss.outroRedOrbRadius = 100.0f;
          }

          if (t >= 10.0f && t < 10.1f && !outroFakeCatActive) {
            InitBossPlayer(&outroFakeCat, (Vector2){1000.0f, 419.0f}, 419.0f);
            outroFakeCat.facingRight = false;
            outroFakeCat.state = PSTATE_WALK;
            outroFakeCatActive = true;
            outroFakeCatAlpha = 1.0f;
            boss.outroYellowOrbActive = true;
            boss.outroYellowOrbRadius = 15.0f;
            boss.outroYellowOrbVel = (Vector2){0, 0};
          }

          if (t >= 10.0f) {
            if (t < 13.5f) {
              outroFakeCat.position.x -= dt * 100.0f;
              outroFakeCat.state = PSTATE_WALK;
              outroFakeCat.frameTimer += dt;
              if (outroFakeCat.frameTimer >= 0.08f) {
                outroFakeCat.frameTimer = 0;
                outroFakeCat.currentFrame =
                    (outroFakeCat.currentFrame + 1) % 12;
              }
              boss.outroYellowOrbPos =
                  (Vector2){outroFakeCat.position.x + 10.0f,
                            outroFakeCat.position.y - 15.0f};
            } else if (t < 14.5f) {
              outroFakeCat.state = PSTATE_IDLE;
              outroFakeCat.frameTimer += dt;
              if (outroFakeCat.frameTimer >= 0.12f) {
                outroFakeCat.frameTimer = 0;
                outroFakeCat.currentFrame =
                    (outroFakeCat.currentFrame + 1) % 10;
              }
              float t_slide = (t - 13.5f) / 1.0f;
              Vector2 startPos = {outroFakeCat.position.x + 10.0f,
                                  outroFakeCat.position.y - 15.0f};
              Vector2 endPos = {outroFakeCat.position.x - 20.0f, 419.0f};
              boss.outroYellowOrbPos.x =
                  startPos.x + (endPos.x - startPos.x) * t_slide;
              boss.outroYellowOrbPos.y =
                  startPos.y + (endPos.y - startPos.y) * t_slide;
            } else if (t < 15.3f) {
              if (outroFakeCat.state != PSTATE_ATTACK) {
                outroFakeCat.state = PSTATE_ATTACK;
                outroFakeCat.currentFrame = 0;
                outroFakeCat.frameTimer = 0.0f;
              }
              outroFakeCat.frameTimer += dt;
              if (outroFakeCat.frameTimer >= 0.10f) {
                outroFakeCat.frameTimer = 0;
                outroFakeCat.currentFrame = (outroFakeCat.currentFrame + 1) % 8;
              }
              if (t >= 15.0f && boss.outroYellowOrbVel.x == 0.0f) {
                Audio_PlaySFX(SFX_SLASH);
                Vector2 dir = {boss.outroRedOrbPos.x - boss.outroYellowOrbPos.x,
                               boss.outroRedOrbPos.y -
                                   boss.outroYellowOrbPos.y};
                float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                boss.outroYellowOrbVel.x = (dir.x / len) * 600.0f;
                boss.outroYellowOrbVel.y = (dir.y / len) * 600.0f;
              }
            } else {
              outroFakeCat.state = PSTATE_IDLE;
              outroFakeCat.frameTimer += dt;
              if (outroFakeCat.frameTimer >= 0.12f) {
                outroFakeCat.frameTimer = 0;
                outroFakeCat.currentFrame =
                    (outroFakeCat.currentFrame + 1) % 10;
              }
            }

            if (boss.outroYellowOrbVel.x != 0.0f && boss.outroYellowOrbActive) {
              boss.outroYellowOrbPos.x += boss.outroYellowOrbVel.x * dt;
              boss.outroYellowOrbPos.y += boss.outroYellowOrbVel.y * dt;
              float dx = boss.outroYellowOrbPos.x - boss.outroRedOrbPos.x;
              float dy = boss.outroYellowOrbPos.y - boss.outroRedOrbPos.y;
              if (sqrtf(dx * dx + dy * dy) < boss.outroRedOrbRadius) {
                boss.ringShockwaveTimer = 0.01f;
                boss.outroAuraActive = true;
                boss.outroAuraScale = 0.0f;
                boss.outroYellowOrbActive = false;
                boss.outroRedOrbActive = false;
                Audio_PlaySFX(SFX_DAMAGE);
                outroFlashTimer = 1.5f;
              }
            }

            if (outroFlashTimer > 0.0f) {
              outroFlashTimer -= dt;
              if (outroFlashTimer < 0.0f)
                outroFlashTimer = 0.0f;
            }

            if (boss.outroAuraActive) {
              boss.outroAuraScale += dt * 3.0f;
              if (boss.outroAuraScale > 2.0f)
                boss.outroAuraActive = false;
            }
          }
        }

        // Boundaries
        if (bossPlayer.position.x < 30)
          bossPlayer.position.x = 30;
        if (bossPlayer.position.x > 1216 - 30)
          bossPlayer.position.x = 1216 - 30;

        // State check
        if (boss.defeated && boss.deathTimer >= 4.0f)
          bossGameState = STATE_WIN;
        if (bossPlayer.hp <= 0)
          bossGameState = STATE_LOSE;
      } else if (bossGameState == STATE_LOSE) {
        if (IsKeyPressed(KEY_R)) {
          // Reset boss fight
          InitBossPlayer(&bossPlayer, (Vector2){200.0f, 419.0f}, 419.0f);
          InitBoss(&boss, bossTargetPos, bossTargetPos);
          boss.scale = 3.7f;
          InitProjectileManager(&pm);
          InitOrbManager(&om);

          Audio_ResetBossFight();

          prevClawActive = false;
          prevLaserActive = false;
          prevHazardCount = 0;
          clawSlashPlayed = false;

          bossGameState = STATE_PLAYING;
          myCam.zoom = 0.88f;
          CameraLookAt(&myCam, bossTargetPos);
        }
      } else if (bossGameState == STATE_WIN) {
        if (IsKeyPressed(KEY_R)) {
          player.loadNextMap = true; // Cycle back to Map 1
        }
      }
    } else {
      gPreIntroSlowWalk = 0;
      // Map 1 or Map 2 normal update
      UpdatePlayer(&player, &gameMap, dt);
      
      bool isMap2 = (strstr(mapList[currentMapIndex], "thesecondmap") != NULL);
      if (isMap2) {
          UpdateRainSystem(&myCam, &gameMap, dt);
      }
      
      if (IsKeyPressed(KEY_R)) {
        player.loadNextMap = true;
      }
    }

    // --- MAP TRANSITION ---
    if (player.loadNextMap) {
      currentMapIndex++;
      if (currentMapIndex >= totalMaps) {
        currentMapIndex =
            0; // Loop back to the first map when you beat the last one!
      }

      // Unload Map 3 assets if they are loaded
      if (bossInitialized) {
        Audio_UnloadBossAssets();
        UnloadTexture(texAgis);
        UnloadProjectileAssets();
        UnloadBossAssets();
        UnloadBoomAssets();
        BossSetArenaMap(NULL, 0.0f, 620.0f);
        OrbSetArenaMap(NULL, 0.0f, 620.0f);
        if (cuteBossMap) {
          MapUnload(cuteBossMap);
          cuteBossMap = NULL;
        }
        bossInitialized = false;
      }

      UnloadMapData(&gameMap);
      gameMap = LoadMapData(mapList[currentMapIndex]);
      player.loadNextMap = false;
      rainInitialized = false;

      if (currentMapIndex == 1) {
        // Lazy load assets for boss fight
        Audio_LoadBossAssets();

        texAgis = LoadTexture("boss/assets/boss/sprites/agis.png");
        cuteBossMap = MapLoad("boss/assets/boss map 1v1.tmj");
        BossSetArenaMap(cuteBossMap, 0.0f, 419.0f);
        OrbSetArenaMap(cuteBossMap, 0.0f, 419.0f);

        bossTargetPos = FindBossPosition(&gameMap, (Vector2){608.0f, 220.0f});
        InitBossPlayer(&bossPlayer, (Vector2){200.0f, 419.0f}, 419.0f);
        InitBoss(&boss, bossTargetPos, bossTargetPos);
        boss.scale = 3.7f;
        InitProjectileManager(&pm);
        InitOrbManager(&om);

        bossInitialized = true;
        prevClawActive = false;
        prevLaserActive = false;
        prevHazardCount = 0;
        clawSlashPlayed = false;
        prevRainActive = false;
        Audio_StopSFX(SFX_ALARM);
        bossGameState = STATE_PLAYING;

        // Reset boss player position
        bossPlayer.position = (Vector2){200.0f, 419.0f};
      } else {
        player.position = FindSpawnPoint(
            &gameMap, (Vector2){50.0f, 1168.0f}); // Teleport to spawn point

        // Reset player states completely
        player.velocity = (Vector2){0, 0};
        player.currentFrame = 0;
        player.isAttacking = false;
        player.isJumping = false;
        player.freezeTimer = 0.0f;
      }

      // Snap camera instantly to new location
      if (currentMapIndex == 1) {
        myCam.zoom = 0.88f;
        CameraLookAt(&myCam, bossTargetPos);
      } else {
        myCam.zoom = 1.50f;
        CameraLookAt(&myCam, player.position);
      }

      // Update bounds for camera just in case the new map has different
      // dimensions
      float bgMinX, bgMaxX, bgMinY, bgMaxY;
      GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
      CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);
    }

    // --- CAMERA UPDATE ---
    if (currentMapIndex == 1) {
      if (bossGameState == STATE_PLAYING) {
        if (boss.state == BOSS_PRE_INTRO) {
          // Focus on player, zoomed in very close (e.g. 2.2f)
          myCam.zoom += (2.2f - myCam.zoom) * 2.0f * dt;
          Vector2 targetPos = {bossPlayer.position.x,
                               bossPlayer.position.y - 50.0f};
          CameraUpdate(&myCam, targetPos, dt);
        } else if (boss.state == BOSS_INTRO) {
          float targetZoom = 1.13f; // base zoom out for arena
          Vector2 targetPos;

          if (boss.introTimer < 5.0f) {
            targetPos = (Vector2){390.0f, 250.0f}; // Left Boom Node
            targetZoom = 1.50f;
          } else if (boss.introTimer < 10.0f) {
            targetPos = (Vector2){890.0f, 250.0f}; // Right Boom Node
            targetZoom = 1.50f;
          } else if (boss.introTimer < 15.0f) {
            targetPos = (Vector2){640.0f, 200.0f}; // Center Boom Node
            targetZoom = 1.50f;
          } else if (boss.introTimer < 35.0f) {
            targetPos = (Vector2){bossPlayer.position.x,
                                  bossPlayer.position.y - 50.0f}; // Player
          } else {
            targetPos = (Vector2){608.0f, 220.0f}; // Boss focus
            targetZoom = 1.4f;
          }

          myCam.zoom += (targetZoom - myCam.zoom) * 2.0f * dt;
          CameraUpdate(&myCam, targetPos, dt);
        } else if (boss.state == BOSS_ROAR) {
          myCam.zoom += (1.45f - myCam.zoom) * 3.0f * dt;
          Vector2 targetPos = boss.position;
          CameraUpdate(&myCam, targetPos, dt);
        } else if (boss.state == BOSS_OUTRO) {
          float targetZoom = 0.95f;
          Vector2 targetPos = {bossPlayer.position.x,
                               bossPlayer.position.y - 70.0f};
          if (boss.outroTimer < 5.0f) {
            targetZoom = 1.8f;
            targetPos = boss.outroRedOrbPos;
          } else if (boss.outroTimer < 15.0f) {
            targetZoom = 1.8f;
            targetPos =
                (Vector2){bossPlayer.position.x, bossPlayer.position.y - 50.0f};
          }
          myCam.zoom += (targetZoom - myCam.zoom) * 2.0f * dt;
          CameraUpdate(&myCam, targetPos, dt);
        } else {
          // Zoom ra xa hơn 1 chút để thấy cả 3 cục boom + cửa thoát (map
          // rộng/cao hơn).
          myCam.zoom += (0.95f - myCam.zoom) * 2.0f * dt;
          Vector2 targetPos = {bossPlayer.position.x,
                               bossPlayer.position.y - 70.0f};
          CameraUpdate(&myCam, targetPos, dt);
        }
      } else {
        CameraUpdate(
            &myCam,
            (Vector2){bossPlayer.position.x, bossPlayer.position.y - 50.0f},
            dt);
      }
    } else {
      CameraUpdate(&myCam, player.position, dt);
    }

    float skyAlpha = 0.0f;
    float mountainAlpha = 0.0f;
    float buildingAlpha = 0.0f;
    float fgAlpha = 0.0f;
    if (currentMapIndex == 1 && bossInitialized &&
        boss.state == BOSS_PRE_INTRO) {
      if (!boss.preIntroTriggered) {
        skyAlpha = 0.95f;
        mountainAlpha = 0.95f;
        buildingAlpha = 0.95f;
        fgAlpha = 0.90f;
      } else {
        float progress = boss.preIntroLightProgress;
        if (progress < 0.25f) {
          skyAlpha = 0.95f * (1.0f - progress / 0.25f);
          mountainAlpha = 0.95f;
          buildingAlpha = 0.95f;
          fgAlpha = 0.90f;
        } else if (progress < 0.50f) {
          skyAlpha = 0.0f;
          mountainAlpha = 0.95f * (1.0f - (progress - 0.25f) / 0.25f);
          buildingAlpha = 0.95f;
          fgAlpha = 0.90f;
        } else if (progress < 0.75f) {
          skyAlpha = 0.0f;
          mountainAlpha = 0.0f;
          buildingAlpha = 0.95f * (1.0f - (progress - 0.50f) / 0.25f);
          fgAlpha = 0.90f;
        } else {
          skyAlpha = 0.0f;
          mountainAlpha = 0.0f;
          buildingAlpha = 0.0f;
          fgAlpha = 0.90f * (1.0f - (progress - 0.75f) / 0.25f);
        }
      }
    }

    // --- DRAW WORLD SPACE (to render texture) ---
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode2D(myCam.rl);

    if (currentMapIndex == 1 && bossInitialized) {
      // === MAP 3 (BOSS FIGHT): 2-pass rendering for correct Z-order ===
      // Pass 1.1: Sky layers (rất xa - parallax factor 0.3, horizontal +
      // vertical, centered relative to map center 608.0f, 419.0f)
      float skyParallaxX = (myCam.rl.target.x - 608.0f) * (1.0f - 0.3f);
      float skyParallaxY = (myCam.rl.target.y - 419.0f) * (1.0f - 0.3f);
      for (int repeat = -1; repeat <= 1; repeat++) {
        float extraX = repeat * 1152.0f; // Sky segment repetition step
        for (int i = 0; i < gameMap.layerCount; i++) {
          TMJLayer *layer = &gameMap.layers[i];
          if (!layer->visible || strstr(layer->name, "sky") == NULL)
            continue;
          if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
            for (int y = 0; y < layer->height; y++) {
              for (int x = 0; x < layer->width; x++) {
                int rawGid = layer->data[y * layer->width + x];
                int gid = rawGid & 0x1FFFFFFF;
                if (gid == 0)
                  continue;
                int tsIdx = -1;
                for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                  if (gid >= gameMap.tilesets[k].firstgid) {
                    tsIdx = k;
                    break;
                  }
                }
                if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0 &&
                    gameMap.tilesets[tsIdx].columns > 0) {
                  TMJTileset *ts = &gameMap.tilesets[tsIdx];
                  int localId = gid - ts->firstgid;
                  int tx = ts->margin + (localId % ts->columns) *
                                            (ts->tileWidth + ts->spacing);
                  int ty = ts->margin + (localId / ts->columns) *
                                            (ts->tileHeight + ts->spacing);
                  unsigned int uGid = (unsigned int)rawGid;
                  bool flipH = (uGid & 0x80000000) != 0;
                  bool flipV = (uGid & 0x40000000) != 0;
                  bool flipD = (uGid & 0x20000000) != 0;

                  float scaleX = 1.0f;
                  float scaleY = 1.0f;
                  float rot = 0.0f;

                  if (flipD) {
                      if (flipH && flipV) {
                          rot = 90.0f;
                          scaleX = -1.0f;
                      } else if (flipH) {
                          rot = 90.0f;
                      } else if (flipV) {
                          rot = 270.0f;
                      } else {
                          rot = 90.0f;
                          scaleY = -1.0f;
                      }
                  } else {
                      if (flipH) scaleX = -1.0f;
                      if (flipV) scaleY = -1.0f;
                  }

                  Rectangle source = {(float)tx, (float)ty,
                                      (float)ts->tileWidth * scaleX,
                                      (float)ts->tileHeight * scaleY};
                  
                  Vector2 pos = {(float)x * gameMap.tileWidth + layer->offsetx +
                                     skyParallaxX + extraX,
                                 (float)y * gameMap.tileHeight +
                                     layer->offsety + skyParallaxY};
                                     
                  Rectangle dest = { pos.x + ts->tileWidth / 2.0f, pos.y + ts->tileHeight / 2.0f, 
                                     (float)ts->tileWidth, (float)ts->tileHeight };
                  Vector2 origin = { (float)ts->tileWidth / 2.0f, (float)ts->tileHeight / 2.0f };

                  DrawTexturePro(ts->texture, source, dest, origin, rot, WHITE);
                }
              }
            }
          }
          if (strcmp(layer->type, "objectgroup") == 0) {
            for (int j = 0; j < layer->objectCount; j++) {
              TMJObject *obj = &layer->objects[j];
              if (!obj->visible || obj->texture.id == 0)
                continue;
              Rectangle source = {0, 0, (float)obj->texture.width,
                                  (float)obj->texture.height};
              if (obj->flipX)
                source.width = -source.width;
              if (obj->flipY)
                source.height = -source.height;
              Rectangle dest = {obj->x + layer->offsetx + skyParallaxX + extraX,
                                obj->y + layer->offsety + skyParallaxY,
                                obj->width, obj->height};
              Vector2 origin = {0, obj->height};
              DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                             Fade(WHITE, layer->opacity * obj->opacity));
            }
          }
        }
      }
      if (boss.state == BOSS_PRE_INTRO && skyAlpha > 0.001f) {
        DrawRectangle(-2000, -2000, 6000, 6000,
                      (Color){0, 0, 0, (unsigned char)(skyAlpha * 255)});
      }

      // Pass 1.2: Mountain layers (xa - parallax factor 0.6, horizontal +
      // vertical, centered relative to map center 608.0f, 419.0f)
      float mountainParallaxX = (myCam.rl.target.x - 608.0f) * (1.0f - 0.6f);
      float mountainParallaxY = (myCam.rl.target.y - 419.0f) * (1.0f - 0.6f);
      for (int repeat = -1; repeat <= 1; repeat++) {
        float extraX = repeat * 1200.0f; // Mountain segment repetition step
        for (int i = 0; i < gameMap.layerCount; i++) {
          TMJLayer *layer = &gameMap.layers[i];
          if (!layer->visible || strstr(layer->name, "mountain") == NULL)
            continue;
          if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
            for (int y = 0; y < layer->height; y++) {
              for (int x = 0; x < layer->width; x++) {
                int gid = layer->data[y * layer->width + x];
                if (gid == 0)
                  continue;
                int tsIdx = -1;
                for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                  if (gid >= gameMap.tilesets[k].firstgid) {
                    tsIdx = k;
                    break;
                  }
                }
                if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0 &&
                    gameMap.tilesets[tsIdx].columns > 0) {
                  TMJTileset *ts = &gameMap.tilesets[tsIdx];
                  int localId = gid - ts->firstgid;
                  int tx = ts->margin + (localId % ts->columns) *
                                            (ts->tileWidth + ts->spacing);
                  int ty = ts->margin + (localId / ts->columns) *
                                            (ts->tileHeight + ts->spacing);
                  Rectangle source = {(float)tx, (float)ty,
                                      (float)ts->tileWidth,
                                      (float)ts->tileHeight};
                  Vector2 pos = {(float)x * gameMap.tileWidth + layer->offsetx +
                                     mountainParallaxX + extraX,
                                 (float)y * gameMap.tileHeight +
                                     layer->offsety + mountainParallaxY};
                  DrawTextureRec(ts->texture, source, pos, WHITE);
                }
              }
            }
          }
          if (strcmp(layer->type, "objectgroup") == 0) {
            for (int j = 0; j < layer->objectCount; j++) {
              TMJObject *obj = &layer->objects[j];
              if (!obj->visible || obj->texture.id == 0)
                continue;
              Rectangle source = {0, 0, (float)obj->texture.width,
                                  (float)obj->texture.height};
              if (obj->flipX)
                source.width = -source.width;
              if (obj->flipY)
                source.height = -source.height;
              Rectangle dest = {obj->x + layer->offsetx + mountainParallaxX +
                                    extraX,
                                obj->y + layer->offsety + mountainParallaxY,
                                obj->width, obj->height};
              Vector2 origin = {0, obj->height};
              DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                             Fade(WHITE, layer->opacity * obj->opacity));
            }
          }
        }
      }
      if (boss.state == BOSS_PRE_INTRO && mountainAlpha > 0.001f) {
        DrawRectangle(-2000, -2000, 6000, 6000,
                      (Color){0, 0, 0, (unsigned char)(mountainAlpha * 255)});
      }

      // Pass 1.3: Building layers (giữ nguyên parallax 1.0)
      for (int i = 0; i < gameMap.layerCount; i++) {
        TMJLayer *layer = &gameMap.layers[i];
        if (!layer->visible || strstr(layer->name, "building") == NULL)
          continue;
        if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
          for (int y = 0; y < layer->height; y++) {
            for (int x = 0; x < layer->width; x++) {
              int rawGid = layer->data[y * layer->width + x];
              int gid = rawGid & 0x1FFFFFFF;
              if (gid == 0)
                continue;
              int tsIdx = -1;
              for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                if (gid >= gameMap.tilesets[k].firstgid) {
                  tsIdx = k;
                  break;
                }
              }
              if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0 &&
                  gameMap.tilesets[tsIdx].columns > 0) {
                TMJTileset *ts = &gameMap.tilesets[tsIdx];
                int localId = gid - ts->firstgid;
                int tx = ts->margin + (localId % ts->columns) *
                                          (ts->tileWidth + ts->spacing);
                int ty = ts->margin + (localId / ts->columns) *
                                          (ts->tileHeight + ts->spacing);
                unsigned int uGid = (unsigned int)rawGid;
                bool flipH = (uGid & 0x80000000) != 0;
                bool flipV = (uGid & 0x40000000) != 0;
                bool flipD = (uGid & 0x20000000) != 0;

                float scaleX = 1.0f;
                float scaleY = 1.0f;
                float rot = 0.0f;

                if (flipD) {
                    if (flipH && flipV) {
                        rot = 90.0f;
                        scaleX = -1.0f;
                    } else if (flipH) {
                        rot = 90.0f;
                    } else if (flipV) {
                        rot = 270.0f;
                    } else {
                        rot = 90.0f;
                        scaleY = -1.0f;
                    }
                } else {
                    if (flipH) scaleX = -1.0f;
                    if (flipV) scaleY = -1.0f;
                }

                Rectangle source = {(float)tx, (float)ty, (float)ts->tileWidth * scaleX,
                                    (float)ts->tileHeight * scaleY};
                                    
                Vector2 pos = {(float)x * gameMap.tileWidth + layer->offsetx,
                               (float)y * gameMap.tileHeight + layer->offsety};
                               
                Rectangle dest = { pos.x + ts->tileWidth / 2.0f, pos.y + ts->tileHeight / 2.0f, 
                                   (float)ts->tileWidth, (float)ts->tileHeight };
                Vector2 origin = { (float)ts->tileWidth / 2.0f, (float)ts->tileHeight / 2.0f };

                DrawTexturePro(ts->texture, source, dest, origin, rot, WHITE);
              }
            }
          }
        }
        if (strcmp(layer->type, "objectgroup") == 0) {
          for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (!obj->visible || obj->texture.id == 0)
              continue;
            Rectangle source = {0, 0, (float)obj->texture.width,
                                (float)obj->texture.height};
            if (obj->flipX)
              source.width = -source.width;
            if (obj->flipY)
              source.height = -source.height;
            Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety,
                              obj->width, obj->height};
            Vector2 origin = {0, obj->height};
            DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                           Fade(WHITE, layer->opacity * obj->opacity));
          }
        }
      }
      if (boss.state == BOSS_PRE_INTRO && buildingAlpha > 0.001f) {
        DrawRectangle(-2000, -2000, 6000, 6000,
                      (Color){0, 0, 0, (unsigned char)(buildingAlpha * 255)});
      }

      // Draw Statue layer (behind boss)
      for (int i = 0; i < gameMap.layerCount; i++) {
        TMJLayer *layer = &gameMap.layers[i];
        if (!layer->visible || strcmp(layer->name, "statue") != 0)
          continue;
        if (strcmp(layer->type, "objectgroup") == 0) {
          for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (!obj->visible || obj->texture.id == 0)
              continue;
            Rectangle source = {0, 0, (float)obj->texture.width,
                                (float)obj->texture.height};
            if (obj->flipX)
              source.width = -source.width;
            if (obj->flipY)
              source.height = -source.height;
            Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety,
                              obj->width, obj->height};
            Vector2 origin = {0, obj->height};
            DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                           Fade(WHITE, layer->opacity * obj->opacity));
          }
        }
      }

      // Draw BOSS BODY between background and foreground platforms
      DrawBossBody(&boss, texAgis, 0.0f);

      // Pass 2: Foreground layers (tiles, props — everything that is NOT
      // background/ground/boss/agis)
      for (int i = 0; i < gameMap.layerCount; i++) {
        TMJLayer *layer = &gameMap.layers[i];
        if (!layer->visible)
          continue;
        // Skip background (vẽ ở pass 1)
        if (strstr(layer->name, "sky") != NULL ||
            strstr(layer->name, "mountain") != NULL ||
            strstr(layer->name, "building") != NULL)
          continue;
        // Skip ground (collision only) / boss / agis (không vẽ sprite map)
        if (strstr(layer->name, "ground") != NULL)
          continue;
        if (strcmp(layer->name, "boss") == 0 ||
            strcmp(layer->name, "agis") == 0 ||
            strcmp(layer->name, "statue") == 0)
          continue;

        if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
          for (int y = 0; y < layer->height; y++) {
            for (int x = 0; x < layer->width; x++) {
              int rawGid = layer->data[y * layer->width + x];
              int gid = rawGid & 0x1FFFFFFF;
              if (gid == 0)
                continue;
              int tsIdx = -1;
              for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                if (gid >= gameMap.tilesets[k].firstgid) {
                  tsIdx = k;
                  break;
                }
              }
              if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0 &&
                  gameMap.tilesets[tsIdx].columns > 0) {
                TMJTileset *ts = &gameMap.tilesets[tsIdx];
                int localId = gid - ts->firstgid;
                int tx = ts->margin + (localId % ts->columns) *
                                          (ts->tileWidth + ts->spacing);
                int ty = ts->margin + (localId / ts->columns) *
                                          (ts->tileHeight + ts->spacing);
                unsigned int uGid = (unsigned int)rawGid;
                bool flipH = (uGid & 0x80000000) != 0;
                bool flipV = (uGid & 0x40000000) != 0;
                bool flipD = (uGid & 0x20000000) != 0;

                float scaleX = 1.0f;
                float scaleY = 1.0f;
                float rot = 0.0f;

                if (flipD) {
                    if (flipH && flipV) {
                        rot = 90.0f;
                        scaleX = -1.0f;
                    } else if (flipH) {
                        rot = 90.0f;
                    } else if (flipV) {
                        rot = 270.0f;
                    } else {
                        rot = 90.0f;
                        scaleY = -1.0f;
                    }
                } else {
                    if (flipH) scaleX = -1.0f;
                    if (flipV) scaleY = -1.0f;
                }

                Rectangle source = {(float)tx, (float)ty, (float)ts->tileWidth * scaleX,
                                    (float)ts->tileHeight * scaleY};
                                    
                Vector2 pos = {(float)x * gameMap.tileWidth + layer->offsetx,
                               (float)y * gameMap.tileHeight + layer->offsety};
                               
                Rectangle dest = { pos.x + ts->tileWidth / 2.0f, pos.y + ts->tileHeight / 2.0f, 
                                   (float)ts->tileWidth, (float)ts->tileHeight };
                Vector2 origin = { (float)ts->tileWidth / 2.0f, (float)ts->tileHeight / 2.0f };

                DrawTexturePro(ts->texture, source, dest, origin, rot, WHITE);
              }
            }
          }
        }
        if (strcmp(layer->type, "objectgroup") == 0) {
          for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (!obj->visible || obj->texture.id == 0)
              continue;
            Rectangle source = {0, 0, (float)obj->texture.width,
                                (float)obj->texture.height};
            if (obj->flipX)
              source.width = -source.width;
            if (obj->flipY)
              source.height = -source.height;
            Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety,
                              obj->width, obj->height};
            Vector2 origin = {0, obj->height};
            DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                           Fade(WHITE, layer->opacity * obj->opacity));
          }
        }
      }

      // Draw Player (under foreground overlay, so it's also darkened)
      DrawBossPlayer(&bossPlayer, texIdle, texWalk, texRun, texJump, texRunJump,
                     texAttack, texHurt);

      // Foreground overlay
      if (boss.state == BOSS_PRE_INTRO && fgAlpha > 0.001f) {
        DrawRectangle(-2000, -2000, 6000, 6000,
                      (Color){0, 0, 0, (unsigned char)(fgAlpha * 255)});
      }

      // (Glowing Beacon visual removed as requested)

      // Boss skills + boom nodes + orbs + projectiles — on top of everything
      DrawBossSkills(&boss);
      DrawBooms(&boss);
      DrawOrbs(&om);
      DrawProjectiles(&pm);

      if (boss.state == BOSS_OUTRO) {
        if (boss.outroRedOrbActive) {
          Texture2D redOrbTex = GetOrbDamageTexture();
          if (redOrbTex.id > 0) {
            float size = boss.outroRedOrbRadius * 2.5f;
            Rectangle source = {0, 0, 128.0f, 128.0f};
            Rectangle dest = {boss.outroRedOrbPos.x, boss.outroRedOrbPos.y,
                              size, size};
            Vector2 origin = {size / 2.0f, size / 2.0f};
            DrawTexturePro(redOrbTex, source, dest, origin,
                           boss.outroTimer * -100.0f,
                           (Color){255, 100, 100, 255});
          } else {
            DrawCircleV(boss.outroRedOrbPos, boss.outroRedOrbRadius,
                        (Color){255, 60, 60, 255});
            DrawCircleLines((int)boss.outroRedOrbPos.x,
                            (int)boss.outroRedOrbPos.y, boss.outroRedOrbRadius,
                            (Color){255, 100, 100, 255});
          }
        }
        if (outroFakeCatActive) {
          if (outroFakeCatAlpha > 0.99f) {
            DrawBossPlayer(&outroFakeCat, texIdle, texWalk, texRun, texJump,
                           texRunJump, texAttack, texHurt);
          } else {
            BeginBlendMode(BLEND_ALPHA);
            Color tint = {255, 255, 255,
                          (unsigned char)(outroFakeCatAlpha * 255)};
            int frameIdx = outroFakeCat.currentFrame % 10;
            if (outroFakeCat.state == PSTATE_WALK)
              frameIdx = outroFakeCat.currentFrame % 12;
            if (outroFakeCat.state == PSTATE_ATTACK)
              frameIdx = outroFakeCat.currentFrame % 8;
            Texture2D curTex = texIdle;
            if (outroFakeCat.state == PSTATE_WALK)
              curTex = texWalk;
            if (outroFakeCat.state == PSTATE_ATTACK)
              curTex = texAttack;
            Rectangle source = {(float)frameIdx * 80, 0, 64, 64};
            if (outroFakeCat.facingRight)
              source.width = -source.width;
            Rectangle dest = {outroFakeCat.position.x,
                              outroFakeCat.position.y + 19.2f, 64.0f * 1.2f,
                              64.0f * 1.2f};
            Vector2 origin = {64.0f * 1.2f / 2.0f, 64.0f * 1.2f};
            DrawTexturePro(curTex, source, dest, origin, 0.0f, tint);
            EndBlendMode();
          }
        }
        if (boss.outroYellowOrbActive) {
          Texture2D yellowOrbTex = GetOrbDamageTexture();
          if (yellowOrbTex.id > 0) {
            float size = boss.outroYellowOrbRadius * 2.5f;
            Rectangle source = {0, 0, 128.0f, 128.0f};
            Rectangle dest = {boss.outroYellowOrbPos.x,
                              boss.outroYellowOrbPos.y, size, size};
            Vector2 origin = {size / 2.0f, size / 2.0f};
            DrawTexturePro(yellowOrbTex, source, dest, origin,
                           boss.outroTimer * 200.0f,
                           (Color){255, 255, 100, 255});
          } else {
            DrawCircleV(boss.outroYellowOrbPos, boss.outroYellowOrbRadius,
                        GOLD);
            DrawCircleLines((int)boss.outroYellowOrbPos.x,
                            (int)boss.outroYellowOrbPos.y,
                            boss.outroYellowOrbRadius, YELLOW);
          }
        }
        if (boss.outroAuraActive) {
          float rad = boss.outroAuraScale * 150.0f;
          float alpha = 1.0f - (boss.outroAuraScale / 2.0f);
          if (alpha < 0.0f)
            alpha = 0.0f;
          DrawCircleGradient(
              (int)boss.outroRedOrbPos.x, (int)boss.outroRedOrbPos.y, rad,
              (Color){255, 255, 255, (unsigned char)(alpha * 200)},
              (Color){255, 200, 100, 0});
        }
      }

    } else {
      // === MAP 1 & 2: single pass (original logic) ===
      bool isMap2 = (strstr(mapList[currentMapIndex], "thesecondmap") != NULL);
      for (int i = 0; i < gameMap.layerCount; i++) {
        TMJLayer *layer = &gameMap.layers[i];
        if (!layer->visible)
          continue;

        float parallaxX = 0.0f;
        if (strcmp(layer->name, "background") == 0 && !isMap2) {
            float originX = 0.0f;
            if (strcmp(layer->type, "objectgroup") == 0 && layer->objectCount > 0) {
                originX = layer->objects[0].x;
                for (int j = 1; j < layer->objectCount; j++) {
                    if (layer->objects[j].x < originX) originX = layer->objects[j].x;
                }
            }
            parallaxX = (myCam.rl.target.x - originX) * (1.0f - 0.2f);
        }

        if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
          for (int y = 0; y < layer->height; y++) {
            for (int x = 0; x < layer->width; x++) {
              int rawGid = layer->data[y * layer->width + x];
              int gid = rawGid & 0x1FFFFFFF;
              if (gid == 0)
                continue;
              int tsIdx = -1;
              for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                if (gid >= gameMap.tilesets[k].firstgid) {
                  tsIdx = k;
                  break;
                }
              }
              if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0 &&
                  gameMap.tilesets[tsIdx].columns > 0) {
                TMJTileset *ts = &gameMap.tilesets[tsIdx];
                int localId = gid - ts->firstgid;
                int tx = ts->margin + (localId % ts->columns) *
                                          (ts->tileWidth + ts->spacing);
                int ty = ts->margin + (localId / ts->columns) *
                                          (ts->tileHeight + ts->spacing);
                unsigned int uGid = (unsigned int)rawGid;
                bool flipH = (uGid & 0x80000000) != 0;
                bool flipV = (uGid & 0x40000000) != 0;
                bool flipD = (uGid & 0x20000000) != 0;

                float scaleX = 1.0f;
                float scaleY = 1.0f;
                float rot = 0.0f;

                if (flipD) {
                    if (flipH && flipV) {
                        rot = 90.0f;
                        scaleX = -1.0f;
                    } else if (flipH) {
                        rot = 90.0f;
                    } else if (flipV) {
                        rot = 270.0f;
                    } else {
                        rot = 90.0f;
                        scaleY = -1.0f;
                    }
                } else {
                    if (flipH) scaleX = -1.0f;
                    if (flipV) scaleY = -1.0f;
                }

                Rectangle source = {(float)tx, (float)ty, (float)ts->tileWidth * scaleX,
                                    (float)ts->tileHeight * scaleY};
                                    
                Vector2 pos = {(float)x * gameMap.tileWidth + layer->offsetx + parallaxX,
                               (float)y * gameMap.tileHeight + layer->offsety};
                               
                Rectangle dest = { pos.x + ts->tileWidth / 2.0f, pos.y + ts->tileHeight / 2.0f, 
                                   (float)ts->tileWidth, (float)ts->tileHeight };
                Vector2 origin = { (float)ts->tileWidth / 2.0f, (float)ts->tileHeight / 2.0f };

                DrawTexturePro(ts->texture, source, dest, origin, rot, WHITE);
              }
            }
          }
          if (strcmp(layer->name, "Tiles") == 0 && isMap2) {
            DrawMainRain();
          }
        }
        if (strcmp(layer->type, "objectgroup") == 0) {
          for (int j = 0; j < layer->objectCount; j++) {
            TMJObject *obj = &layer->objects[j];
            if (!obj->visible || obj->texture.id == 0)
              continue;
            Rectangle source = {0, 0, (float)obj->texture.width,
                                (float)obj->texture.height};
            if (obj->flipX)
              source.width = -source.width;
            if (obj->flipY)
              source.height = -source.height;
            Rectangle dest = {obj->x + layer->offsetx + parallaxX, obj->y + layer->offsety,
                              obj->width, obj->height};
            Vector2 origin = {0, obj->height};
            DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                           Fade(WHITE, layer->opacity * obj->opacity));
          }
        }
      }
      DrawPlayer(&player, texIdle, texWalk, texRun, texJump, texAttack,
                 texRunJump, texHurt, 64, 64, 0.78f);
      if (isMap2) {
        DrawForegroundRain();
      }
    }

    EndMode2D();
    EndTextureMode();

    // --- DRAW SCREEN SPACE (UI) ---
    BeginDrawing();
    ClearBackground(BLACK);

    Rectangle sourceRec = {0.0f, 0.0f, (float)target.texture.width,
                           (float)-target.texture.height};
    Rectangle destRec = {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};

    bool useGrayscale =
        (currentMapIndex == 1 && bossInitialized &&
         ((boss.state == BOSS_INTRO && boss.introTimer >= 35.0f &&
           boss.introTimer < 41.5f) ||
          (boss.state == BOSS_OUTRO && boss.outroTimer >= 21.5f &&
           boss.outroTimer < 40.5f)));
    bool useShockwave = (currentMapIndex == 1 && bossInitialized &&
                         boss.ringShockwaveTimer > 0.0f);

    if (useGrayscale) {
      BeginShaderMode(grayscaleShader);
    } else if (useShockwave) {
      Vector2 swWorldPos = boss.position;
      if (boss.state == BOSS_OUTRO)
        swWorldPos = boss.outroRedOrbPos;
      Vector2 screenCenter = GetWorldToScreen2D(swWorldPos, myCam.rl);
      float center[2] = {screenCenter.x / VIRTUAL_WIDTH,
                         1.0f - (screenCenter.y / VIRTUAL_HEIGHT)};
      SetShaderValue(shockwaveShader, swCenterLoc, center, SHADER_UNIFORM_VEC2);

      float normalizedTime = boss.ringShockwaveTimer / 1.5f;
      SetShaderValue(shockwaveShader, swTimeLoc, &normalizedTime,
                     SHADER_UNIFORM_FLOAT);

      float params[3] = {10.0f, 0.8f, 0.1f};
      SetShaderValue(shockwaveShader, swParamsLoc, params, SHADER_UNIFORM_VEC3);

      BeginShaderMode(shockwaveShader);
    }

    DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f,
                   WHITE);

    if (useGrayscale || useShockwave)
      EndShaderMode();

    if (currentMapIndex == 1 && bossInitialized) {
      if (boss.state == BOSS_FIGHTING || boss.state == BOSS_DYING ||
          boss.state == BOSS_DEFEATED || boss.state == BOSS_OUTRO) {
        DrawUI(bossPlayer.hp, boss.hp, boss.maxHp);

        const char *phaseText = "Phase 1";
        if (boss.phase == BOSS_PHASE_2)
          phaseText = "Phase 2 - Enraged";
        if (boss.phase == BOSS_PHASE_3)
          phaseText = "Phase 3 - Danger!";
        if (boss.phase == BOSS_PHASE_4)
          phaseText = "Phase 4 - FINAL FORM!";

        int textW = MeasureText(phaseText, 24);
        DrawText(phaseText, SCREEN_WIDTH / 2 - textW / 2, 70, 24, YELLOW);
      }

      // === OUTRO CUTSCENE OVERLAY ===
      if (boss.state == BOSS_OUTRO) {

        // Cinematic bars from the beginning
        DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
        DrawRectangle(0, SCREEN_HEIGHT - 80, SCREEN_WIDTH, 80,
                      (Color){0, 0, 0, 200});

        if (outroFlashTimer > 0.0f) {
          float a = outroFlashTimer / 1.5f;
          if (a > 1.0f)
            a = 1.0f;
          DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                        (Color){255, 255, 255, (unsigned char)(a * 255)});
        }
      }

      if (boss.state == BOSS_INTRO || boss.state == BOSS_OUTRO) {
        if (fmod(GetTime(), 1.0) < 0.5) {
          int skipW = MeasureText("Press ENTER to skip", 20);
          DrawText("Press ENTER to skip", SCREEN_WIDTH - skipW - 20,
                   SCREEN_HEIGHT - 40, 20, (Color){200, 200, 200, 200});
        }
      }

      if (boss.state == BOSS_INTRO) {
        float alpha = boss.introTimer / 3.0f;
        if (alpha > 1.0f)
          alpha = 1.0f;

        int textW = MeasureText("Something approaches...", 36);
        DrawText("Something approaches...", SCREEN_WIDTH / 2 - textW / 2, 120,
                 36, (Color){255, 100, 100, (unsigned char)(alpha * 200)});

        DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
        DrawRectangle(0, SCREEN_HEIGHT - 80, SCREEN_WIDTH, 80,
                      (Color){0, 0, 0, 200});

        float pct = boss.introTimer / 15.5f;
        if (pct > 1.0f)
          pct = 1.0f;
        DrawRectangle(40, SCREEN_HEIGHT - 50, (int)((SCREEN_WIDTH - 80) * pct),
                      6, (Color){255, 100, 100, 150});
      }

      if (boss.state == BOSS_ROAR) {
        float time = (float)GetTime();
        int fontSize = (int)(72.0f + sinf(time * 15.0f) * 10.0f);
        int textW = MeasureText("A G I S", fontSize);
        Color textColor =
            (Color){255, (unsigned char)(100.0f + sinf(time * 20.0f) * 100.0f),
                    50, 255};

        DrawText("A G I S", SCREEN_WIDTH / 2 - textW / 2,
                 SCREEN_HEIGHT / 2 - fontSize / 2, fontSize, textColor);
        DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
        DrawRectangle(0, SCREEN_HEIGHT - 80, SCREEN_WIDTH, 80,
                      (Color){0, 0, 0, 200});
      }

      if (boss.state == BOSS_FIGHTING) {
        DrawText("A/D: Move | Space: Jump | LEFT-CLICK to PARRY the glowing "
                 "orb back at AGIS!",
                 40, SCREEN_HEIGHT - 50, 22, (Color){200, 200, 200, 200});
      }

      // (Fake death text removed)

      Audio_DrawSubtitles();

      if (bossGameState == STATE_WIN)
        DrawWinScreen();
      if (bossGameState == STATE_LOSE)
        DrawLoseScreen();
    }

    DrawFPS(10, 10);
    EndDrawing();
  }

  // --- CLEANUP ---
  if (bossInitialized) {
    Audio_UnloadBossAssets();
    UnloadTexture(texAgis);
    UnloadProjectileAssets();
    UnloadBossAssets();
    UnloadBoomAssets();
    BossSetArenaMap(NULL, 0.0f, 620.0f);
    OrbSetArenaMap(NULL, 0.0f, 620.0f);
    if (cuteBossMap) {
      MapUnload(cuteBossMap);
      cuteBossMap = NULL;
    }
  }
  Audio_CloseDevice();

  UnloadTexture(texIdle);
  UnloadTexture(texWalk);
  UnloadTexture(texRun);
  UnloadTexture(texJump);
  UnloadTexture(texAttack);
  UnloadTexture(texRunJump);
  UnloadTexture(texHurt);
  UnloadMapData(&gameMap);
  UnloadRenderTexture(target);
  UnloadFont(gGameFont);
  CloseWindow();
  return 0;
}
