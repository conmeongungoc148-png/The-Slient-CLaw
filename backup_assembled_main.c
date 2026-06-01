#include "raylib.h"
#include "player.h"
#include "boss.h"
#include "projectile.h"
#include "orb.h"
#include "collision.h"
#include "gamestate.h"
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define GROUND_Y 620.0f
#define BOSS_X (SCREEN_WIDTH / 2)
#define BOSS_DEFAULT_SCALE 3.0f

static float cameraShake = 0.0f;

static void DrawForestArena(cute_tiled_map_t *map, Texture2D fallbackTiles);
static float GetArenaOffsetY(cute_tiled_map_t *map);
static float GetBossCenterYForFeet(float feetY);
static Vector2 GetMapObjectOrDefault(cute_tiled_map_t *map, const char *name, float offsetY, Vector2 fallback);

// === Background Music System ===
typedef enum {
    BG_NONE = -1,
    BG_PHRASE12 = 0,
    BG_PHRASE3 = 1,
    BG_PHRASE4 = 2,
    BG_ENDING = 3
} BgTrack;

static Music bgTracks[4];
static BgTrack currentBgTrack = BG_NONE;
static BgTrack pendingBgTrack = BG_NONE;
static float bgVolume = 0.0f;
static float bgVolumeTarget = 1.0f;
static float bgFadeSpeed = 2.0f;  // Volume change per second
static bool fadingOut = false;
static bool endingTriggered = false;
static BossPhase lastPhase = BOSS_PHASE_1;

static BgTrack GetTrackForPhase(BossPhase phase) {
    switch (phase) {
        case BOSS_PHASE_1:
        case BOSS_PHASE_2: return BG_PHRASE12;
        c
                 "height":432,
                 "id":4,
                 "name":"",
                 "opacity":1,
                 "rotation":0,
                 "type":"",
                 "visible":true,
                 "width":101.647,
                 "x":288,
                 "y":433
                }, 
                {
                 "gid":105,
                 "height":432,
                 "id":5,
                 "name":"",
                 "opacity":1,
                 "rotation":0,
                 "type":"",
                 "visible":true,
                 "width":101.647,
                 "x":384,
                 "y":433
                }, 
                {
                 "gid":105,
                 "height":432,
                 "id":6,
                 "name":"",
                 "opacity":1,
                 "rotation":0,
                 "type":"",
                 "visible":true,
                 "width":101.647,
                 "x":480,
                 "y":433
                }, 
                {
                 "gid":105,
                 "height":432,
                 "id":7,
                 "name":"",
                 "opacity":1,
                 "rotation":0,
                 "type":"",
                 "visible":true,
                 "width":101.647,
                 "x":576,
                 "y":433
    };
}

typedef enum {
    BG_NONE = -1,
    BG_PHRASE12 = 0,
    BG_PHRASE3 = 1,
    BG_PHRASE4 = 2,
    BG_ENDING = 3
} BgTrack;

static BgTrack GetTrackForPhase(BossPhase phase) {
    switch (phase) {
        case BOSS_PHASE_1:
        case BOSS_PHASE_2: return BG_PHRASE12;
    boss->slamWarningTime = 0.0f;
    boss->slamTimer = 0.0f;
    boss->slamPos = (Vector2){0, 0};
    boss->shockwaveRadius = 0.0f;
}

static void LoadBossBattleAssets(Texture2D *texAgis, Texture2D *texCatIdle, Texture2D *texCatWalk,
                                  Texture2D *texCatRun, Texture2D *texCatJump, Texture2D *texCatAttack,
                                  Texture2D *texCatHurt, Texture2D *texForestTiles, cute_tiled_map_t **forestMap,
                                  float *arenaOffsetY, Music *introMusic, Music bgTracks[4], Sound *laughSfx,
                                  Sound *damageSfx, Sound *alarmSfx, Sound *hitsSfx, Sound *slashSfx) {
    *texAgis = LoadTexture("assets/boss/sprites/agis.png");
    *texCatIdle = LoadTexture("assets/boss/sprites/cat/IDLE.png");
    *texCatWalk = LoadTexture("assets/boss/sprites/cat/WALK.png");
    *texCatRun = LoadTexture("assets/boss/sprites/cat/RUN.png");
    *texCatJump = LoadTexture("assets/boss/sprites/cat/JUMP.png");
    *texCatAttack = LoadTexture("assets/boss/sprites/cat/ATTACK 1.png");
    *texCatHurt = LoadTexture("assets/boss/sprites/cat/HURT.png");
    *texForestTiles = LoadTexture("asset_sources/boss_arena/Fort of Illusion Files/Assets/Layers/tileset.png");
    *forestMap = MapLoad("assets/boss map 1v1.tmj");
    
    if (*forestMap) {
        *arenaOffsetY = BOSS_ARENA_GROUND_Y - MapGetGroundY(*forestMap);
    } else {
        *arenaOffsetY = 0.0f;
    }

    BossSetArenaMap(*forestMap, *arenaOffsetY, BOSS_ARENA_GROUND_Y);
    OrbSetArenaMap(*forestMap, *arenaOffsetY, BOSS_ARENA_GROUND_Y);

    *introMusic = LoadMusicStream("assets/audio/music/start.ogg");
    (*introMusic).looping = false;

    bgTracks[0] = LoadMusicStream("assets/audio/music/background/phrase1and2.ogg");
    bgTracks[1] = LoadMusicStream("assets/audio/music/background/phrase3.ogg");
    bgTracks[2] = LoadMusicStream("assets/audio/music/background/phrase4.ogg");
    bgTracks[3] = LoadMusicStream("assets/audio/music/background/ending.ogg");
    
    bgTracks[0].looping = true;
    bgTracks[1].looping = true;
    bgTracks[2].looping = true;
    bgTracks[3].looping = false;
    SetMusicVolume(*introMusic, 1.0f);
    for (int i = 0; i < 4; i++) {
        SetMusicVolume(bgTracks[i], 1.0f);
    }

    *laughSfx = LoadSound("assets/audio/sfx/laugh.ogg");
    *damageSfx = LoadSound("assets/audio/sfx/damage.ogg");
    *alarmSfx = LoadSound("assets/audio/sfx/alarm.ogg");
    *hitsSfx = LoadSound("assets/audio/sfx/hits.ogg");
    *slashSfx = LoadSound("assets/audio/sfx/slash.ogg");
    SetSoundVolume(*laughSfx, 1.0f);
    SetSoundVolume(*damageSfx, 1.0f);
    SetSoundVolume(*alarmSfx, 1.0f);
    SetSoundVolume(*hitsSfx, 1.0f);
    SetSoundVolume(*slashSfx, 1.0f);
}

        if (MapFindObject(arenaMap, "spawn", &pos, &rect)) {
            playerSpawn = (Vector2){pos.x, pos.y + arenaOffsetY};
        }
        if (FindLargestMapObject(arenaMap, "boss", &pos, &rect)) {
            float bossX = rect.width > 0.0f ? pos.x + rect.width * 0.5f : pos.x;
            bossFeet = (Vector2){bossX, pos.y + arenaOffsetY};
            if (rect.width > 0.0f && rect.height > 0.0f) {
                float scaleX = rect.width / (float)BOSS_FRAME_W;
                float scaleY = rect.height / (float)BOSS_FRAME_H;
                bossScale = (scaleX + scaleY) * 0.5f;
            }
        }
    }

    Vector2 bossSpawn = {bossFeet.x, GetBossCenterYForFeet(bossFeet.y, bossScale)};
    InitBossPlayer(player, playerSpawn, groundY);
    InitBoss(boss, (Vector2){bossSpawn.x, 900.0f}, bossSpawn);
    boss->scale = bossScale;
    InitProjectileManager(pm);
    InitOrbManager(om);
}

static void DrawForestArena(cute_tiled_map_t *map, float arenaOffsetY, Texture2D fallbackTiles) {
    if (!map) {
        DrawRectangleGradientV(0, 0, 1280, 720, 
            (Color){30, 10, 50, 255},
            (Color){10, 5, 20, 255});
        DrawRectangle(0, 620, 1280, 100, (Color){60, 40, 80, 255});
        DrawRectangle(0, 616, 1280, 4, (Color){150, 80, 180, 255});
        DrawRectangle(0, 0, 20, 720, (Color){40, 20, 60, 255});
        DrawRectangle(1260, 0, 20, 720, (Color){40, 20, 60, 255});
        for (int x = 0; x < 1280; x += 80) {
            DrawLine(x, 620, x, 720, (Color){80, 50, 100, 100});
        }
        return;
    }

    cute_tiled_layer_t *layer = map->layers;
    while (layer) {
        if (layer->visible) {
            const char *layerName = layer->name.ptr ? layer->name.ptr : "";
            if (!TextContainsNoCaseLocal(layerName, "boss") &&
                !TextContainsNoCaseLocal(layerName, "agis")) {
                MapDrawLayerEx(map, layerName, 0.0f, arenaOffsetY, fallbackTiles);
            }
        }
        layer = layer->next;
    }

    DrawRectangle(0, 620, 1280, 100, (Color){8, 10, 16, 70});
}

Vector2 FindSpawnPoint(GameMap *map, Vector2 defaultPos) {
    for (int i = 0; i < map->layerCount; i++) {
        if (strcmp(map->layers[i].type, "objectgroup") == 0) {
            for (int j = 0; j < map->layers[i].objectCount; j++) {
                TMJObject *obj = &map->layers[i].objects[j];
                if (TextIsEqual(obj->name, "spawn") || TextIsEqual(obj->name, "Spawn") ||
                    TextIsEqual(map->layers[i].name, "spawn") || TextIsEqual(map->layers[i].name, "Spawn")) {
                    float spawnX = obj->x + map->layers[i].offsetx;
                    float spawnY = obj->y + map->layers[i].offsety + obj->height - 64.0f;
                camera.zoom += (1.0f - camera.zoom) * 2.0f * dt;
                Vector2 normalTarget = { SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
                camera.target.x += (normalTarget.x - camera.target.x) * 2.0f * dt;
                camera.target.y += (normalTarget.y - camera.target.y) * 2.0f * dt;
        }
    }
    TraceLog(LOG_WARNING, "[SPAWN] Spawn point NOT found! Using default: %.2f, %.2f", defaultPos.x, defaultPos.y);
    return defaultPos;
}

void GetMapBackgroundBounds(GameMap *map, float *minX, float *maxX, float *minY, float *maxY) {
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
        if (!layer->visible) continue;

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

                    if (left < bgMinX) bgMinX = left;
                    if (right > bgMaxX) bgMaxX = right;
                    if (top < bgMinY) bgMinY = top;
                    if (bottom > bgMaxY) bgMaxY = bottom;
                    foundBackground = true;
                }
            }
        }
    }

    if (foundBackground) {
        if (bgMinX < 0.0f) bgMinX = 0.0f;
        if (bgMaxX > *maxX) bgMaxX = *maxX;
        if (bgMinY < 0.0f) bgMinY = 0.0f;
        if (bgMaxY > *maxY) bgMaxY = *maxY;

        *minX = bgMinX;
        *maxX = bgMaxX;
        *minY = bgMinY;
        *maxY = bgMaxY;
    }
}

int main(void) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
             "The Forest - Full Screen Infinite Map");
    InitAudioDevice();
  SetMasterVolume(1.0f);
  SetTargetFPS(60);

  // Boss battle state and assets (only loaded/used on map 2)
  cute_tiled_map_t *forestMap = NULL;
  float arenaOffsetY = 0.0f;
  BossPlayer bossPlayer = {0};
  Boss boss = {0};
  ProjectileManager pm = {0};
  OrbManager om = {0};
  
  // Boss textures
  Texture2D texAgis = {0};
  Texture2D texCatIdle = {0};
  Texture2D texCatWalk = {0};
  Texture2D texCatRun = {0};
  Texture2D texCatJump = {0};
  Texture2D texCatAttack = {0};
  Texture2D texCatHurt = {0};
  Texture2D texForestTiles = {0};

  // Boss audio
  Music bossIntroMusic = {0};
  Music bossBgTracks[4] = {0};
  Sound bossLaughSfx = {0};
  Sound bossDamageSfx = {0};
  Sound bossAlarmSfx = {0};
  Sound bossHitsSfx = {0};
  Sound bossSlashSfx = {0};

  // Boss audio flags
  bool bossIntroMusicStarted = false;
  bool bossLaughPlayed = false;
  int currentBossBgTrack = -1; // BG_NONE
  int pendingBossBgTrack = -1;
  float bgVolume = 0.0f;
  float bgVolumeTarget = 1.0f;
  float bgFadeSpeed = 2.0f;
  bool fadingOut = false;
  bool endingTriggered = false;
  int lastPhase = 0; // BOSS_PHASE_1
  bool prevClawActive = false;
  bool prevLaserActive = false;
  int prevHazardCount = 0;
  bool clawSlashPlayed = false;
  
  GameState bossGameState = STATE_PLAYING;
  float cameraShake = 0.0f;
  MyCamera bossCamera = {0};

  RenderTexture2D target = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

  const char *mapList[] = {
      "assets/forestmap.tmj",
      "assets/nightcity.tmj",
      "assets/boss map 1v1.tmj"
  };
  // Fallback spawn nếu map không có spawn point object
  // Boss map: sàn chính y=419, nhân vật cao 64px → đứng tại y = 419 - 64 = 355
  const Vector2 mapSpawnDefaults[] = {
      {50.0f,  1168.0f},  // forestmap
      {50.0f,  1168.0f},  // nightcity
      {100.0f,  355.0f},  // boss map 1v1
  };
  int totalMaps = 3;
  int currentMapIndex = 0;

  GameMap gameMap = LoadMapData(mapList[currentMapIndex]);

  // Find spawn point from the map, or fallback to default
  Vector2 startPos = FindSpawnPoint(&gameMap, mapSpawnDefaults[currentMapIndex]);

  Player player = {0};
  InitPlayer(&player, startPos);

  Texture2D texIdle = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/IDLE.png");
  Texture2D texWalk = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/WALK.png");
  Texture2D texRun = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/RUN.png");
  Texture2D texJump = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/JUMP.png");
  Texture2D texAttack = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/ATTACK 1.png");
  Texture2D texRunJump = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/RUNNING JUMP.png");
  Texture2D texHurt = LoadTexture(
      "asset_sources/player_cat/FREE_Cat 2D Pixel Art/Sprites/HURT.png");

  if (currentMapIndex == 2) {
      LoadBossBattleAssets(&texAgis, &texCatIdle, &texCatWalk, &texCatRun, &texCatJump, &texCatAttack, &texCatHurt,
                           &texForestTiles, &forestMap, &arenaOffsetY, &bossIntroMusic, bossBgTracks,
                           &bossLaughSfx, &bossDamageSfx, &bossAlarmSfx, &bossHitsSfx, &bossSlashSfx);
      InitBossBattle(&bossPlayer, &boss, &pm, &om, forestMap, arenaOffsetY);
      BossCutsceneReset(&bossCamera, forestMap, arenaOffsetY, GetBossArenaCenter(forestMap, arenaOffsetY), VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
      bossGameState = STATE_PLAYING;
  }

  MyCamera myCam = CameraNew(player.position.x, player.position.y,
                             VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  myCam.zoom = 1.42f;
  CameraSetSmoothDamped(&myCam, 10.0f);
  float bgMinX, bgMaxX, bgMinY, bgMaxY;
  GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
  CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.016f; // Cap dt to prevent physics glitches when loading a map takes time!

    if (currentMapIndex == 2) {
        // === UPDATE MUSIC STREAMS ===
        UpdateMusicStream(bossIntroMusic);
        if (currentBossBgTrack != -1) {
            UpdateMusicStream(bossBgTracks[currentBossBgTrack]);
        }

        // Start intro music
        if (boss.state == BOSS_INTRO && !bossIntroMusicStarted) {
            PlayMusicStream(bossIntroMusic);
            bossIntroMusicStarted = true;
        }

        // Play laugh SFX
        if (boss.state == BOSS_ROAR && !bossLaughPlayed) {
            PlaySound(bossLaughSfx);
            bossLaughPlayed = true;
        }

        // Background music system
    UnloadTexture(texCatIdle);
    UnloadTexture(texCatWalk);
    UnloadTexture(texCatRun);
    UnloadTexture(texCatJump);
    UnloadTexture(texCatAttack);
    UnloadTexture(texCatHurt);
    UnloadTexture(texForestTiles);
    if (forestMap) MapUnload(forestMap);

    UnloadMusicStream(introMusic);
    for (int i = 0; i < 4; i++) {
        UnloadMusicStream(bgTracks[i]);
    }
    UnloadSound(laughSfx);
    UnloadSound(damageSfx);
    UnloadSound(alarmSfx);
    UnloadSound(hitsSfx);
    UnloadSound(slashSfx);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

static void DrawForestArena(cute_tiled_map_t *map, Texture2D fallbackTiles) {
    if (!map) {
        DrawFallbackArena();
        return;
    }

    float offsetY = GetArenaOffsetY(map);
    cute_tiled_layer_t *layer = map->layers;
    while (layer) {
        if (layer->visible) {
            MapDrawLayerEx(map, layer->name.ptr, 0.0f, offsetY, fallbackTiles);
        }
        layer = layer->next;
    }

    DrawRectangle(0, (int)GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)GROUND_Y,
        (Color){8, 10, 16, 70});
}

static float GetArenaOffsetY(cute_tiled_map_t *map) {
    if (!map) return 0.0f;
    return GROUND_Y - MapGetGroundY(map);
}

static float GetBossCenterYForFeet(float feetY) {
    return feetY - ((float)BOSS_FRAME_H * BOSS_DEFAULT_SCALE * 0.5f);
}

static Vector2 GetMapObjectOrDefault(cute_tiled_map_t *map, const char *name, float offsetY, Vector2 fallback) {
    Vector2 pos = {0};
    Rectangle rect = {0};
    if (!MapFindObject(map, name, &pos, &rect)) return fallback;

    return (Vector2){pos.x, pos.y + offsetY};
}

        if (currentBossBgTrack == BG_ENDING && !fadingOut && bgVolume > 0.5f) {
            float played = GetMusicTimePlayed(bossBgTracks[BG_ENDING]);
            float total = GetMusicTimeLength(bossBgTracks[BG_ENDING]);
            if (total > 0 && played >= total - 0.2f) {
                boss.defeated = true;
                boss.state = BOSS_DEFEATED;
            }
        }

        if (bossGameState == STATE_PLAYING) {
            UpdateBoss(&boss, bossPlayer.position, &pm, &om, dt, &cameraShake);

            BossCutsceneUpdate(&bossCamera, &boss, GetBossArenaCenter(forestMap, arenaOffsetY), cameraShake, dt);

            // Check win/lose
            if (boss.defeated && boss.deathTimer >= 4.0f) bossGameState = STATE_WIN;
            if (bossPlayer.hp <= 0) bossGameState = STATE_LOSE;

            if (boss.state == BOSS_FIGHTING) {
                UpdateBossPlayerOnMap(&bossPlayer, forestMap, arenaOffsetY, BOSS_ARENA_GROUND_Y, dt);
                UpdateProjectiles(&pm, dt);
                UpdateOrbs(&om, bossPlayer.position, boss.position, BOSS_ARENA_GROUND_Y, dt);

                // Alarm sound
                if (boss.clawActive && !prevClawActive) {
                    PlaySound(bossAlarmSfx);
                }
                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0 && !clawSlashPlayed) {
                    StopSound(bossAlarmSfx);
                    PlaySound(bossSlashSfx);
                    clawSlashPlayed = true;
                }
                if (!boss.clawActive) clawSlashPlayed = false;
                prevClawActive = boss.clawActive;

                if (boss.laserActive && !prevLaserActive) {
                    PlaySound(bossAlarmSfx);
                }
                if (prevLaserActive && boss.laserActive && boss.laserChargeTime <= 0) {
                    StopSound(bossAlarmSfx);
                }
                prevLaserActive = boss.laserActive;

                if (boss.hazardCount > 0 && prevHazardCount == 0) {
                    PlaySound(bossAlarmSfx);
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
                    StopSound(bossAlarmSfx);
                    prevHazardCount = 0;
                }

                // Catch orb
                {
                    bool wasReady[MAX_ORBS];
                    for (int i = 0; i < MAX_ORBS; i++) {
                        wasReady[i] = (om.orbs[i].state == ORB_READY);
                    }
                    TryCatchOrb(&om, bossPlayer.hurtBox, boss.position);
                    for (int i = 0; i < MAX_ORBS; i++) {
                        if (wasReady[i] && om.orbs[i].state == ORB_RETURNING) {
                            PlaySound(bossHitsSfx);
                        }
                    }
                }

                // Projectile collisions
                for (int i = 0; i < MAX_PROJECTILES; i++) {
                    if (!pm.projectiles[i].active) continue;
                    if (CheckCollision(pm.projectiles[i].hitbox, bossPlayer.hurtBox)) {
                        PlayerTakeDamage(&bossPlayer, pm.projectiles[i].damage);
                        pm.projectiles[i].active = false;
                        pm.count--;
                    }
                }

                // Orb returning hits boss
                for (int i = 0; i < MAX_ORBS; i++) {
                    if (om.orbs[i].state != ORB_RETURNING) continue;
                    if (CheckCollision(om.orbs[i].hitbox, boss.hurtBox)) {
                        BossTakeDamage(&boss, om.orbs[i].damage);
                        PlaySound(bossDamageSfx);
                        om.orbs[i].state = ORB_INACTIVE;
                        boss.orbActive = false;
                    }
                }

                if (boss.laserActive && boss.laserChargeTime <= 0) {
                    if (CheckPlayerInLaser(bossPlayer.position, boss.laserStart, boss.laserEnd, 20.0f)) {
                        PlayerTakeDamage(&bossPlayer, 1);
                    }
                }

                if (boss.slamActive && boss.shockwaveRadius > 50.0f) {
                    if (CheckPlayerInShockwave(bossPlayer.position, boss.slamPos, boss.shockwaveRadius)) {
                        PlayerTakeDamage(&bossPlayer, 2);
                    }
                }

                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0) {
                    if (CheckPlayerInClawZone(bossPlayer.position, boss.clawZone)) {
                        PlayerTakeDamage(&bossPlayer, 2);
                    }
                }

                for (int i = 0; i < boss.hazardCount; i++) {
                    if (!boss.hazardActive[i]) continue;
                    Rectangle hazardRect = {
                        boss.hazardPositions[i].x - 15,
                        boss.hazardPositions[i].y - 80,
                        30, 80
                    };
                    if (CheckCollision(hazardRect, bossPlayer.hurtBox)) {
                        PlayerTakeDamage(&bossPlayer, 1);
                    }
                }

                if (bossPlayer.position.x < 30) bossPlayer.position.x = 30;
                float arenaWidth = GetBossArenaWidth(forestMap);
                if (bossPlayer.position.x > arenaWidth - 30) bossPlayer.position.x = arenaWidth - 30;
            }
        } else {
            if (IsKeyPressed(KEY_R)) {
                if (bossGameState == STATE_WIN) {
                    // Loop back to map 0!
                    currentMapIndex = 0;
                    // Unload boss assets
                    if (forestMap != NULL) {
                        UnloadBossBattleAssets(&texAgis, &texCatIdle, &texCatWalk, &texCatRun, &texCatJump, &texCatAttack, &texCatHurt,
                                               &texForestTiles, &forestMap, &bossIntroMusic, bossBgTracks,
                                               &bossLaughSfx, &bossDamageSfx, &bossAlarmSfx, &bossHitsSfx, &bossSlashSfx);
                    }
                    // Load normal map 0
                    gameMap = LoadMapData(mapList[currentMapIndex]);
                    player.position = FindSpawnPoint(&gameMap, mapSpawnDefaults[currentMapIndex]);
                    player.velocity = (Vector2){0, 0};
                    player.currentFrame = 0;
                    player.isAttacking = false;
                    player.isJumping = false;
                    player.freezeTimer = 0.0f;
                    player.loadNextMap = false;

                    // Camera set
                    CameraLookAt(&myCam, player.position);
                    float bgMinX, bgMaxX, bgMinY, bgMaxY;
                    GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
                    CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);
                } else {
                    // Lose -> Restart boss fight
                    InitBossBattle(&bossPlayer, &boss, &pm, &om, forestMap, arenaOffsetY);
                    bossGameState = STATE_PLAYING;
                    BossCutsceneReset(&bossCamera, GetBossArenaCenter(forestMap, arenaOffsetY), VIRTUAL_WIDTH, VIRTUAL_HEIGHT);

                    StopMusicStream(bossIntroMusic);
                    if (currentBossBgTrack != -1) {
                        StopMusicStream(bossBgTracks[currentBossBgTrack]);
                    }
                    currentBossBgTrack = -1;
                    pendingBossBgTrack = -1;
                    bgVolume = 0.0f;
                    bgVolumeTarget = 1.0f;
                    fadingOut = false;
                    endingTriggered = false;
                    lastPhase = BOSS_PHASE_1;
                    
                    bossIntroMusicStarted = false;
                    bossLaughPlayed = false;
                    prevClawActive = false;
                    prevLaserActive = false;
                    prevHazardCount = 0;
                    clawSlashPlayed = false;
                }
            }
        }
    } else {
        // === STANDARD PLATFORMER LOGIC ===
        UpdatePlayer(&player, &gameMap, dt);
        UpdateSkills(&player, dt);

        if (IsKeyPressed(KEY_Q)) {
          CastSkill(&player, "assets/skills/redfire.tmj");
        }
        if (IsKeyPressed(KEY_E)) {
          CastSkill(&player, "assets/skills/redclaw.tmj");
        }

        if (IsKeyPressed(KEY_R)) {
            player.loadNextMap = true;
        }

        if (player.loadNextMap) {
            currentMapIndex++;
            if (currentMapIndex >= totalMaps) {
                currentMapIndex = 0; // Loop back to the first map when you beat the last one!
            }

            UnloadMapData(&gameMap);
            player.loadNextMap = false;

            if (currentMapIndex == 2) {
                // Initialize Boss Battle!
                LoadBossBattleAssets(&texAgis, &texCatIdle, &texCatWalk, &texCatRun, &texCatJump, &texCatAttack, &texCatHurt,
                                     &texForestTiles, &forestMap, &arenaOffsetY, &bossIntroMusic, bossBgTracks,
                                     &bossLaughSfx, &bossDamageSfx, &bossAlarmSfx, &bossHitsSfx, &bossSlashSfx);
                InitBossBattle(&bossPlayer, &boss, &pm, &om, forestMap, arenaOffsetY);
                BossCutsceneReset(&bossCamera, GetBossArenaCenter(forestMap, arenaOffsetY), VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
                bossGameState = STATE_PLAYING;

                bossIntroMusicStarted = false;
                bossLaughPlayed = false;
                currentBossBgTrack = -1;
                pendingBossBgTrack = -1;
                bgVolume = 0.0f;
                bgVolumeTarget = 1.0f;
                fadingOut = false;
                endingTriggered = false;
                lastPhase = BOSS_PHASE_1;
                prevClawActive = false;
                prevLaserActive = false;
                prevHazardCount = 0;
                clawSlashPlayed = false;
            } else {
                gameMap = LoadMapData(mapList[currentMapIndex]);
                player.position = FindSpawnPoint(&gameMap, mapSpawnDefaults[currentMapIndex]); // Teleport to spawn point
                
                // Reset player states completely
                player.velocity = (Vector2){0, 0};
                player.currentFrame = 0;
                player.isAttacking = false;
                player.isJumping = false;
                player.freezeTimer = 0.0f;

                // Snap camera instantly to new location
                CameraLookAt(&myCam, player.position);
                
                // Update bounds for camera just in case the new map has different dimensions
                float bgMinX, bgMaxX, bgMinY, bgMaxY;
                GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
                CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);
            }
        }

        CameraUpdate(&myCam, player.position, dt);
    }

    BeginTextureMode(target);
    ClearBackground(BLACK);

    if (currentMapIndex == 2) {
        // === DRAW BOSS FIGHT ===
        BeginMode2D(bossCamera.rl);
        DrawForestArena(forestMap, arenaOffsetY, texForestTiles);
        DrawBoss(&boss, texAgis, 0);
        DrawOrbs(&om);
        DrawProjectiles(&pm);
        DrawBossPlayer(&bossPlayer, texCatIdle, texCatWalk, texCatRun, texCatJump, texCatAttack, texCatHurt);
        EndMode2D();

        // UI
        if (boss.state == BOSS_FIGHTING || boss.state == BOSS_DYING || boss.state == BOSS_DEFEATED) {
            DrawUI(bossPlayer.hp, boss.hp, boss.maxHp);

            const char *phaseText = "Phase 1";
            if (boss.phase == BOSS_PHASE_2) phaseText = "Phase 2 - Enraged";
            if (boss.phase == BOSS_PHASE_3) phaseText = "Phase 3 - Danger!";
            if (boss.phase == BOSS_PHASE_4) phaseText = "Phase 4 - FINAL!";
            DrawText(phaseText, VIRTUAL_WIDTH / 2 - 80, 65, 18, YELLOW);
        }

        if (boss.state == BOSS_INTRO) {
            float alpha = boss.introTimer / 3.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            DrawText("Something approaches...", VIRTUAL_WIDTH/2 - 150, 80, 24, 
                (Color){255, 100, 100, (unsigned char)(alpha * 200)});
            
            DrawRectangle(0, 0, VIRTUAL_WIDTH, 60, (Color){0, 0, 0, 200});
            DrawRectangle(0, VIRTUAL_HEIGHT - 60, VIRTUAL_WIDTH, 60, (Color){0, 0, 0, 200});
            
            float pct = boss.introTimer / 15.5f;
            if (pct > 1.0f) pct = 1.0f;
            DrawRectangle(20, VIRTUAL_HEIGHT - 40, (int)(200 * pct), 4, (Color){255, 100, 100, 150});
        }

        if (boss.state == BOSS_ROAR) {
            DrawText("A G I S", VIRTUAL_WIDTH/2 - 80, VIRTUAL_HEIGHT/2 - 50, 48, 
                (Color){255, 50, 50, 255});
            DrawRectangle(0, 0, VIRTUAL_WIDTH, 60, (Color){0, 0, 0, 200});
            DrawRectangle(0, VIRTUAL_HEIGHT - 60, VIRTUAL_WIDTH, 60, (Color){0, 0, 0, 200});
        }

        if (boss.state == BOSS_FIGHTING) {
            DrawText("A/D: Move  |  Space: Jump  |  Catch yellow orbs to damage boss!", 
                20, VIRTUAL_HEIGHT - 30, 16, (Color){200, 200, 200, 200});
        }

        if (boss.state == BOSS_DYING) {
            DrawText("FINAL BLOW!", VIRTUAL_WIDTH/2 - 100, VIRTUAL_HEIGHT/2, 36, 
                (Color){255, 255, 100, 255});
        }

        BossCutsceneDrawOverlay(&boss, VIRTUAL_WIDTH, VIRTUAL_HEIGHT);

        if (bossGameState == STATE_WIN) D
        return;
    }

    float shakeX = 0, shakeY = 0;
    if (boss->shakeTimer > 0) {
        shakeX = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
        shakeY = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
          TMJLayer *layer = &gameMap.layers[i];
          if (!layer->visible) continue;
          char lname[64];
          strncpy(lname, layer->name, 63); lname[63] = '\0';
          for (int c = 0; lname[c]; c++) if (lname[c] >= 'A' && lname[c] <= 'Z') lname[c] += 32;
          if (strstr(lname, "background") == NULL && strstr(lname, "sky") == NULL && strstr(lname, "mountain") == NULL)
            continue;
          if (strcmp(layer->type, "objectgroup") == 0) {
            for (int j = 0; j < layer->objectCount; j++) {
              TMJObject *obj = &layer->objects[j];
              if (!obj->visible || obj->texture.id == 0) continue;
              Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
              if (obj->flipX) source.width = -source.width;
              if (obj->flipY) source.height = -source.height;
              Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
              Vector2 origin = {0, obj->height};
              DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                             Fade(WHITE, layer->opacity * obj->opacity));
            }
          }
        }

        // Pass 2: Vẽ tất cả layers còn lại theo thứ tự
        for (int i = 0; i < gameMap.layerCount; i++) {
          TMJLayer *layer = &gameMap.layers[i];
          if (!layer->visible) continue;

          // Tên layer lowercase để so sánh
          char lowerName[64];
          strncpy(lowerName, layer->name, 63); lowerName[63] = '\0';
          for (int c = 0; lowerName[c]; c++) if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') lowerName[c] += 32;

          // Skip background layers (đã vẽ ở pass 1)
          if (strstr(lowerName, "background") != NULL || strstr(lowerName, "sky") != NULL || strstr(lowerName, "mountain") != NULL)
            continue;

          // Skip layer "ground" - chỉ dùng cho collision
          if (strstr(lowerName, "ground") != NULL) {
            if (IsKeyDown(KEY_G)) {
              for (int j = 0; j < layer->objectCount; j++) {
                TMJObject *obj = &layer->objects[j];
                DrawRectangleLines((int)(obj->x + layer->offsetx),
                                   (int)(obj->y + layer->offsety),
                                   (int)obj->width, (int)obj->height, GREEN);
              }
            }
            continue;
          }

          // --- VẼ TILE LAYER ---
          if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
              for (int y = 0; y < layer->height; y++) {
                  for (int x = 0; x < layer->width; x++) {
                      int gid = layer->data[y * layer->width + x];
                      if (gid == 0) continue;
                      
                      // Tìm tileset phù hợp cho gid này
                      int tsIdx = -1;
                      for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                          if (gid >= gameMap.tilesets[k].firstgid) {
                              tsIdx = k;
                              break;
                          }
                      }

                      if (tsIdx != -1 && gameMap.tilesets[tsIdx].texture.id != 0) {
                          TMJTileset *ts = &gameMap.tilesets[tsIdx];
                          int localId = gid - ts->firstgid;
                          int tx = ts->margin + (localId % ts->columns) * (ts->tileWidth + ts->spacing);
                          int ty = ts->margin + (localId / ts->columns) * (ts->tileHeight + ts->spacing);
                          
                          Rectangle source = { (float)tx, (float)ty, (float)ts->tileWidth, (float)ts->tileHeight };
                          Vector2 pos = { (float)x * gameMap.tileWidth + layer->offsetx, (float)y * gameMap.tileHeight + layer->offsety };
                          DrawTextureRec(ts->texture, source, pos, WHITE);
                      }
                  }
              }
 
                
                // Tiled dùng bottom-left cho GID objects
                Vector2 origin = {0, obj->height};
                
                DrawTexturePro(obj->texture, source, dest, origin, obj->rotation,
                               Fade(WHITE, layer->opacity * obj->opacity));
              }
          }
        }

        DrawPlayer(&player, texIdle, texWalk, texRun, texJump, texAttack, texRunJump, texHurt, 64, 64, 0.78f);
        DrawSkills(&player);

        EndMode2D();
    }

    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle sourceRec = {0.0f, 0.0f, (float)target.texture.width,
                           (float)-target.texture.height};
    Rectangle destRec = {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
    DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f,
                   WHITE);
    DrawFPS(10, 10);
    EndDrawing();
  }

  UnloadTexture(texIdle);
  UnloadTexture(texWalk);
  UnloadTexture(texRun);
  UnloadTexture(texJump);
  UnloadTexture(texAttack);
  UnloadTexture(texRunJump);
  UnloadTexture(texHurt);
  UnloadMapData(&gameMap);
  UnloadRenderTexture(target);
  if (forestMap != NULL) {
      UnloadBossBattleAssets(&texAgis, &texCatIdle, &texCatWalk, &texCatRun, &texCatJump, &texCatAttack, &texCatHurt,
                             &texForestTiles, &forestMap, &bossIntroMusic, bossBgTracks,
                             &bossLaughSfx, &bossDamageSfx, &bossAlarmSfx, &bossHitsSfx, &bossSlashSfx);
  }
  CloseAudioDevice();
  CloseWindow();
  return 0;
}

