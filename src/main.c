#include "camera.h"
#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "boss.h"
#include "boss_player.h"
#include "projectile.h"
#include "orb.h"
#include "collision.h"
#include "gamestate.h"
#include "map.h"

// === Boss Fight State & Assets ===
static bool bossInitialized = false;
static BossPlayer bossPlayer;
static Boss boss;
static ProjectileManager pm;
static OrbManager om;
static cute_tiled_map_t *cuteBossMap = NULL;
static GameState bossGameState = STATE_PLAYING;

typedef enum {
    BG_NONE = -1,
    BG_PHRASE12 = 0,
    BG_PHRASE3 = 1,
    BG_PHRASE4 = 2,
    BG_ENDING = 3
} BgTrack;

static Music bgTracks[4];
static Music introMusic;
static Sound laughSfx;
static Sound damageSfx;
static Sound alarmSfx;
static Sound hitsSfx;
static Sound slashSfx;
static Texture2D texAgis;

static BgTrack currentBgTrack = BG_NONE;
static BgTrack pendingBgTrack = BG_NONE;
static float bgVolume = 0.0f;
static float bgVolumeTarget = 1.0f;
static float bgFadeSpeed = 2.0f;  // Volume change per second
static bool fadingOut = false;
static bool endingTriggered = false;
static BossPhase lastPhase = BOSS_PHASE_1;

static bool prevClawActive = false;
static bool prevLaserActive = false;
static int prevHazardCount = 0;
static bool clawSlashPlayed = false;
static bool prevRainActive = false;
static bool introMusicStarted = false;
static bool laughPlayed = false;

static BgTrack GetTrackForPhase(BossPhase phase) {
    switch (phase) {
        case BOSS_PHASE_1:
        case BOSS_PHASE_2: return BG_PHRASE12;
        case BOSS_PHASE_3: return BG_PHRASE3;
        case BOSS_PHASE_4: return BG_PHRASE4;
    }
    return BG_PHRASE12;
}

static void SwitchBgTrack(BgTrack newTrack) {
    if (newTrack == currentBgTrack) return;
    pendingBgTrack = newTrack;
    fadingOut = true;
    bgVolumeTarget = 0.0f;
}

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
                    TraceLog(LOG_INFO, "[BOSS] Found boss object in layer '%s' at center: %.2f, %.2f",
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
                if (TextIsEqual(obj->name, "spawn") || TextIsEqual(obj->name, "Spawn") ||
                    TextIsEqual(map->layers[i].name, "spawn") || TextIsEqual(map->layers[i].name, "Spawn")) {
                    float spawnX = obj->x + map->layers[i].offsetx;
                    float spawnY = obj->y + map->layers[i].offsety + obj->height - 64.0f;
                    TraceLog(LOG_INFO, "[SPAWN] Found spawn point at: %.2f, %.2f", spawnX, spawnY);
                    return (Vector2){spawnX, spawnY};
                }
            }
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

Font gGameFont;

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
             "The Forest - Full Screen Infinite Map");
  gGameFont = LoadFontEx("assets/other/MedievalSharp-Regular.ttf", 96, NULL, 0);
  SetTextureFilter(gGameFont.texture, TEXTURE_FILTER_BILINEAR);
  SetTargetFPS(60);

   InitAudioDevice();

  RenderTexture2D target = LoadRenderTexture(VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

  const char *mapList[] = {
      "boss/assets/forestmap.tmj",
      "boss/assets/nightcity.tmj",
      "boss/assets/boss map 1v1.tmj"
  };
  int totalMaps = 3;
  int currentMapIndex = 0;

  GameMap gameMap = LoadMapData(mapList[currentMapIndex]);

  // Find spawn point from the map, or fallback to default
  Vector2 startPos = FindSpawnPoint(&gameMap, (Vector2){50.0f, 1168.0f});

  Player player = {0};
  InitPlayer(&player, startPos);

  Texture2D texIdle = LoadTexture(
      "assets/cat/IDLE.png");
  Texture2D texWalk = LoadTexture(
      "assets/cat/WALK.png");
  Texture2D texRun = LoadTexture(
      "assets/cat/RUN.png");
  Texture2D texJump = LoadTexture(
      "assets/cat/JUMP.png");
  Texture2D texAttack = LoadTexture(
      "assets/cat/ATTACK 1.png");
  Texture2D texRunJump = LoadTexture(
      "assets/cat/RUNNING JUMP.png");
  Texture2D texHurt = LoadTexture(
      "assets/cat/HURT.png");

  MyCamera myCam = CameraNew(player.position.x, player.position.y,
                             VIRTUAL_WIDTH, VIRTUAL_HEIGHT);
  myCam.zoom = 1.0f;
  CameraSetSmoothDamped(&myCam, 10.0f);
  float bgMinX, bgMaxX, bgMinY, bgMaxY;
  GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
  CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    if (dt > 0.1f) dt = 0.016f; // Cap dt to prevent physics glitches when loading a map takes time!

    // --- UPDATE ---
    if (currentMapIndex == 2) {
        // === UPDATE MUSIC STREAMS ===
        if (bossInitialized) {
            UpdateMusicStream(introMusic);
            if (currentBgTrack != BG_NONE) {
                UpdateMusicStream(bgTracks[currentBgTrack]);
            }
            
            // Bắt đầu phát intro music
            if (boss.state == BOSS_INTRO && !introMusicStarted) {
                PlayMusicStream(introMusic);
                introMusicStarted = true;
            }

            // Phát tiếng cười khi vào ROAR
            if (boss.state == BOSS_ROAR && !laughPlayed) {
                PlaySound(laughSfx);
                laughPlayed = true;
            }

            // === BACKGROUND MUSIC SYSTEM ===
            if (boss.state == BOSS_FIGHTING && currentBgTrack == BG_NONE && !fadingOut) {
                BgTrack startTrack = GetTrackForPhase(boss.phase);
                currentBgTrack = startTrack;
                PlayMusicStream(bgTracks[currentBgTrack]);
                bgVolume = 0.0f;
                bgVolumeTarget = 1.0f;
                SetMusicVolume(bgTracks[currentBgTrack], bgVolume);
                lastPhase = boss.phase;
            }

            if (boss.state == BOSS_FIGHTING && currentBgTrack != BG_NONE) {
                if (boss.phase != lastPhase) {
                    BgTrack desiredTrack = GetTrackForPhase(boss.phase);
                    if (desiredTrack != currentBgTrack && !fadingOut) {
                        SwitchBgTrack(desiredTrack);
                    }
                    lastPhase = boss.phase;
                }
            }

            if (boss.state == BOSS_DYING && !endingTriggered) {
                if (currentBgTrack != BG_ENDING && !fadingOut) {
                    SwitchBgTrack(BG_ENDING);
                }
                endingTriggered = true;
            }

            float volumeStep = bgFadeSpeed * dt;
            if (bgVolume < bgVolumeTarget) {
                bgVolume += volumeStep;
                if (bgVolume > bgVolumeTarget) bgVolume = bgVolumeTarget;
            } else if (bgVolume > bgVolumeTarget) {
                bgVolume -= volumeStep;
                if (bgVolume < bgVolumeTarget) bgVolume = bgVolumeTarget;
            }

            if (currentBgTrack != BG_NONE) {
                SetMusicVolume(bgTracks[currentBgTrack], bgVolume);
            }

            if (fadingOut && bgVolume <= 0.001f) {
                if (currentBgTrack != BG_NONE) {
                    StopMusicStream(bgTracks[currentBgTrack]);
                }
                currentBgTrack = pendingBgTrack;
                pendingBgTrack = BG_NONE;
                fadingOut = false;
                if (currentBgTrack != BG_NONE) {
                    SetMusicVolume(bgTracks[currentBgTrack], 0.0f);
                    PlayMusicStream(bgTracks[currentBgTrack]);
                    bgVolumeTarget = 1.0f;
                }
            }

            if (currentBgTrack == BG_ENDING && !fadingOut && bgVolume > 0.5f) {
                float played = GetMusicTimePlayed(bgTracks[BG_ENDING]);
                float total = GetMusicTimeLength(bgTracks[BG_ENDING]);
                if (total > 0 && played >= total - 0.2f) {
                    boss.defeated = true;
                    boss.state = BOSS_DEFEATED;
                }
            }
        }

        // === BOSS FIGHT LOGIC UPDATE ===
        if (bossGameState == STATE_PLAYING) {
            // Đi-bộ-only: pre-intro HOẶC giai đoạn đi ra cửa (boss giả chết, cửa mở).
            gPreIntroSlowWalk = (boss.state == BOSS_PRE_INTRO) ||
                                (boss.state == BOSS_FAKE_DEATH && boss.walkAwayDoorActive);
            static float activeCameraShake = 0.0f;
            float bossCameraShake = 0.0f;
            UpdateBoss(&boss, bossPlayer.position, &pm, &om, dt, &bossCameraShake);
            
            bool bossSetsShakeEveryFrame = (boss.state == BOSS_INTRO) || (boss.state == BOSS_ROAR) || (boss.state == BOSS_FAKE_DEATH) || (boss.state == BOSS_DYING);
            if (bossSetsShakeEveryFrame) {
                activeCameraShake = bossCameraShake;
            } else {
                if (bossCameraShake > 0.0f) {
                    if (bossCameraShake > activeCameraShake) {
                        activeCameraShake = bossCameraShake;
                    }
                } else {
                    activeCameraShake -= dt * 40.0f;
                    if (activeCameraShake < 0.0f) activeCameraShake = 0.0f;
                }
            }
            
            if (activeCameraShake > 0.0f) {
                CameraShake(&myCam, 0.05f, activeCameraShake);
            }
            
            // Player update logic (with lock check)
            bool isPlayerLocked = (boss.state == BOSS_PRE_INTRO && boss.preIntroTriggered) || (boss.state == BOSS_INTRO) || (boss.state == BOSS_ROAR);

            if (isPlayerLocked) {
                bossPlayer.velocity = (Vector2){0, 0};
                bossPlayer.state = PSTATE_IDLE;
                bossPlayer.isJumping = false;
                bossPlayer.isRunning = false;
                bossPlayer.isSprinting = false;
                bossPlayer.isAttacking = false;
                bossPlayer.isHurt = false;
                bossPlayer.position.x = boss.beaconPos.x;
                bossPlayer.frameTimer += dt;
                if (bossPlayer.frameTimer >= 0.12f) {
                    bossPlayer.frameTimer = 0.0f;
                    bossPlayer.currentFrame = (bossPlayer.currentFrame + 1) % 10;
                }
                bossPlayer.hurtBox.x = bossPlayer.position.x - 10;
                bossPlayer.hurtBox.y = bossPlayer.position.y - 30;
            } else {
                if (boss.state == BOSS_PRE_INTRO || boss.state == BOSS_FIGHTING ||
                    boss.state == BOSS_TRUE_ENRAGE ||
                    (boss.state == BOSS_FAKE_DEATH && boss.walkAwayDoorActive)) {
                    UpdateBossPlayerOnMap(&bossPlayer, cuteBossMap, 0.0f, 419.0f, dt);
                    bossPlayer.hurtBox.x = bossPlayer.position.x - 10;
                    bossPlayer.hurtBox.y = bossPlayer.position.y - 30;
                }
            }

            // === ĐI RA CỬA: player tới mép phải (gần cửa) -> boss bất ngờ trồi lên ===
            if (boss.state == BOSS_FAKE_DEATH && boss.walkAwayDoorActive) {
                if (bossPlayer.position.x >= 1060.0f) {
                    boss.walkAwayDoorActive = false;
                    boss.reviveWalkTimer = 0.0001f;  // bật giai đoạn REVIVAL
                    PlaySound(laughSfx);
                }
            }

            if (boss.state == BOSS_FIGHTING || boss.state == BOSS_TRUE_ENRAGE) {
                UpdateProjectiles(&pm, dt);
                UpdateOrbs(&om, bossPlayer.position, boss.position, 419.0f, dt);
                
                // Audio Alarm, Claw, etc.
                if (boss.clawActive && !prevClawActive) {
                    PlaySound(alarmSfx);
                }
                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0 && !clawSlashPlayed) {
                    StopSound(alarmSfx);
                    PlaySound(slashSfx);
                    clawSlashPlayed = true;
                }
                if (!boss.clawActive) clawSlashPlayed = false;
                prevClawActive = boss.clawActive;

                if (boss.laserActive && !prevLaserActive) {
                    PlaySound(alarmSfx);
                }
                if (prevLaserActive && boss.laserActive && boss.laserChargeTime <= 0) {
                    StopSound(alarmSfx);
                }
                prevLaserActive = boss.laserActive;

                if (boss.hazardCount > 0 && prevHazardCount == 0) {
                    PlaySound(alarmSfx);
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
                    StopSound(alarmSfx);
                    prevHazardCount = 0;
                }

                if (boss.rainActive && !prevRainActive) {
                    PlaySound(alarmSfx);
                }
                if (prevRainActive && boss.rainActive && boss.rainWarningTime <= 0) {
                    StopSound(alarmSfx);
                }
                prevRainActive = boss.rainActive;

                // Orb catch (Player collects orb on floor and it flies to boom node or boss)
                {
                    bool wasReady[MAX_ORBS];
                    for (int i = 0; i < MAX_ORBS; i++) {
                        wasReady[i] = (om.orbs[i].state == ORB_READY);
                    }
                    Vector2 orbTarget = boss.position;
                    if (boss.state != BOSS_TRUE_ENRAGE) {
                        int bi = BoomNearestActive(&boss, bossPlayer.position);
                        if (bi >= 0) {
                            orbTarget = boss.booms[bi].position;
                        }
                    }
                    TryCatchOrb(&om, bossPlayer.hurtBox, orbTarget);
                    for (int i = 0; i < MAX_ORBS; i++) {
                        if (wasReady[i] && om.orbs[i].state == ORB_RETURNING) {
                            PlaySound(hitsSfx);
                        }
                    }
                }

                // Projectile damage to player
                for (int i = 0; i < MAX_PROJECTILES; i++) {
                    if (!pm.projectiles[i].active) continue;
                    if (CheckCollision(pm.projectiles[i].hitbox, bossPlayer.hurtBox)) {
                        BossPlayerTakeDamage(&bossPlayer, pm.projectiles[i].damage);
                        pm.projectiles[i].active = false;
                        pm.count--;
                    }
                }

                // Parry orb (RETURNING) — trong FIGHTING đập vào CỤC BOOM gần nhất;
                // trong TRUE_ENRAGE thì trúng boss = kết liễu thật.
                for (int i = 0; i < MAX_ORBS; i++) {
                    if (om.orbs[i].state != ORB_RETURNING) continue;
                    if (boss.state == BOSS_TRUE_ENRAGE) {
                        if (CheckCollision(om.orbs[i].hitbox, boss.hurtBox)) {
                            BossTakeDamage(&boss, om.orbs[i].damage);
                            PlaySound(damageSfx);
                            om.orbs[i].state = ORB_INACTIVE;
                        }
                    } else {
                        int bi = BoomNearestActive(&boss, om.orbs[i].position);
                        if (bi >= 0) {
                            float dx = boss.booms[bi].position.x - om.orbs[i].position.x;
                            float dy = boss.booms[bi].position.y - om.orbs[i].position.y;
                            if (dx*dx + dy*dy < 70.0f*70.0f) {
                                BoomHit(&boss, bi);
                                PlaySound(damageSfx);
                                om.orbs[i].state = ORB_INACTIVE;
                            }
                        }
                    }
                }

                // Ring damage-orb (từ cục boom) chạm player -> mất máu
                if (CheckPlayerInBoomRings(&boss, bossPlayer.position)) {
                    BossPlayerTakeDamage(&bossPlayer, 1);
                }

                // Other hazards damage
                if (boss.laserActive && boss.laserChargeTime <= 0) {
                    if (CheckPlayerInLaser(bossPlayer.position, boss.laserStart, boss.laserEnd, 20.0f)) {
                        BossPlayerTakeDamage(&bossPlayer, 1);
                    }
                }
                if (boss.slamActive && boss.shockwaveRadius > 50.0f) {
                    if (CheckPlayerInShockwave(bossPlayer.position, boss.slamPos, boss.shockwaveRadius)) {
                        BossPlayerTakeDamage(&bossPlayer, 2);
                    }
                }
                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0) {
                    if (CheckPlayerInClawZone(bossPlayer.position, boss.clawZone)) {
                        BossPlayerTakeDamage(&bossPlayer, 2);
                    }
                }
                for (int i = 0; i < boss.hazardCount; i++) {
                    if (!boss.hazardActive[i]) continue;
                    Rectangle hazardRect = {
                        boss.hazardPositions[i].x - 128.0f,
                        boss.hazardPositions[i].y - 384.0f,
                        256.0f, 384.0f
                    };
                    if (CheckCollision(hazardRect, bossPlayer.hurtBox)) {
                        BossPlayerTakeDamage(&bossPlayer, 1);
                    }
                }
            } else {
                StopSound(alarmSfx);
            }

            // Boundaries
            if (bossPlayer.position.x < 30) bossPlayer.position.x = 30;
            if (bossPlayer.position.x > 1216 - 30) bossPlayer.position.x = 1216 - 30;

            // State check
            if (boss.defeated && boss.deathTimer >= 4.0f) bossGameState = STATE_WIN;
            if (bossPlayer.hp <= 0) bossGameState = STATE_LOSE;
        } else if (bossGameState == STATE_LOSE) {
            if (IsKeyPressed(KEY_R)) {
                // Reset boss fight
                InitBossPlayer(&bossPlayer, (Vector2){200.0f, 419.0f}, 419.0f);
                InitBoss(&boss, (Vector2){bossTargetPos.x, bossTargetPos.y + 680.0f}, bossTargetPos);
                boss.scale = 3.7f;
                InitProjectileManager(&pm);
                InitOrbManager(&om);
                
                StopMusicStream(introMusic);
                if (currentBgTrack != BG_NONE) StopMusicStream(bgTracks[currentBgTrack]);
                currentBgTrack = BG_NONE;
                pendingBgTrack = BG_NONE;
                bgVolume = 0.0f;
                bgVolumeTarget = 1.0f;
                fadingOut = false;
                endingTriggered = false;
                lastPhase = BOSS_PHASE_1;
                
                introMusicStarted = false;
                laughPlayed = false;
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
        if (IsKeyPressed(KEY_R)) {
            player.loadNextMap = true;
        }
    }

    // --- MAP TRANSITION ---
    if (player.loadNextMap) {
        currentMapIndex++;
        if (currentMapIndex >= totalMaps) {
            currentMapIndex = 0; // Loop back to the first map when you beat the last one!
        }

        // Unload Map 3 assets if they are loaded
        if (bossInitialized) {
            UnloadMusicStream(introMusic);
            for (int i = 0; i < 4; i++) {
                UnloadMusicStream(bgTracks[i]);
            }
            UnloadSound(laughSfx);
            UnloadSound(damageSfx);
            UnloadSound(alarmSfx);
            UnloadSound(hitsSfx);
            UnloadSound(slashSfx);
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

        if (currentMapIndex == 2) {
            // Lazy load assets for boss fight
            introMusic = LoadMusicStream("boss/assets/audio/music/start.ogg");
            introMusic.looping = false;
            bgTracks[BG_PHRASE12] = LoadMusicStream("boss/assets/audio/music/background/phrase1and2.ogg");
            bgTracks[BG_PHRASE3] = LoadMusicStream("boss/assets/audio/music/background/phrase3.ogg");
            bgTracks[BG_PHRASE4] = LoadMusicStream("boss/assets/audio/music/background/phrase4.ogg");
            bgTracks[BG_ENDING] = LoadMusicStream("boss/assets/audio/music/background/ending.ogg");
            bgTracks[BG_PHRASE12].looping = true;
            bgTracks[BG_PHRASE3].looping = true;
            bgTracks[BG_PHRASE4].looping = true;
            bgTracks[BG_ENDING].looping = false;

            laughSfx = LoadSound("boss/assets/audio/sfx/laugh.ogg");
            damageSfx = LoadSound("boss/assets/audio/sfx/damage.ogg");
            alarmSfx = LoadSound("boss/assets/audio/sfx/alarm.ogg");
            hitsSfx = LoadSound("boss/assets/audio/sfx/hits.ogg");
            slashSfx = LoadSound("boss/assets/audio/sfx/slash.ogg");

            texAgis = LoadTexture("boss/assets/boss/sprites/agis.png");
            cuteBossMap = MapLoad("boss/assets/boss map 1v1.tmj");
            BossSetArenaMap(cuteBossMap, 0.0f, 419.0f);
            OrbSetArenaMap(cuteBossMap, 0.0f, 419.0f);

            bossTargetPos = FindBossPosition(&gameMap, (Vector2){608.0f, 220.0f});
            InitBossPlayer(&bossPlayer, (Vector2){200.0f, 419.0f}, 419.0f);
            InitBoss(&boss, (Vector2){bossTargetPos.x, bossTargetPos.y + 680.0f}, bossTargetPos);
            boss.scale = 3.7f;
            InitProjectileManager(&pm);
            InitOrbManager(&om);

            bossInitialized = true;
            introMusicStarted = false;
            laughPlayed = false;
            currentBgTrack = BG_NONE;
            pendingBgTrack = BG_NONE;
            bgVolume = 0.0f;
            bgVolumeTarget = 1.0f;
            fadingOut = false;
            endingTriggered = false;
            lastPhase = BOSS_PHASE_1;
            prevClawActive = false;
            prevLaserActive = false;
            prevHazardCount = 0;
            clawSlashPlayed = false;
            prevRainActive = false;
            StopSound(alarmSfx);
            bossGameState = STATE_PLAYING;

            // Reset boss player position
            bossPlayer.position = (Vector2){200.0f, 419.0f};
        } else {
            player.position = FindSpawnPoint(&gameMap, (Vector2){50.0f, 1168.0f}); // Teleport to spawn point
            
            // Reset player states completely
            player.velocity = (Vector2){0, 0};
            player.currentFrame = 0;
            player.isAttacking = false;
            player.isJumping = false;
            player.freezeTimer = 0.0f;
        }

        // Snap camera instantly to new location
        if (currentMapIndex == 2) {
            myCam.zoom = 0.88f;
            CameraLookAt(&myCam, bossTargetPos);
        } else {
            myCam.zoom = 1.0f;
            CameraLookAt(&myCam, player.position);
        }
        
        // Update bounds for camera just in case the new map has different dimensions
        float bgMinX, bgMaxX, bgMinY, bgMaxY;
        GetMapBackgroundBounds(&gameMap, &bgMinX, &bgMaxX, &bgMinY, &bgMaxY);
        CameraSetBounds(&myCam, bgMinX, bgMinY, bgMaxX - bgMinX, bgMaxY - bgMinY);
    }

    // --- CAMERA UPDATE ---
    if (currentMapIndex == 2) {
        if (bossGameState == STATE_PLAYING) {
            if (boss.state == BOSS_PRE_INTRO) {
                // Focus on player, zoomed in very close (e.g. 2.2f)
                myCam.zoom += (2.2f - myCam.zoom) * 2.0f * dt;
                Vector2 targetPos = { bossPlayer.position.x, bossPlayer.position.y - 50.0f };
                CameraUpdate(&myCam, targetPos, dt);
            }
            else if (boss.state == BOSS_INTRO) {
                float progress = boss.introTimer / 15.5f;
                if (progress > 1.0f) progress = 1.0f;
                
                float targetZoom = 2.2f - 1.07f * progress; // Smoothly zoom out from 2.2f to 1.13f
                myCam.zoom += (targetZoom - myCam.zoom) * 2.0f * dt;
                
                Vector2 targetPos = boss.position;
                CameraUpdate(&myCam, targetPos, dt);
            }
            else if (boss.state == BOSS_ROAR) {
                myCam.zoom += (1.45f - myCam.zoom) * 3.0f * dt;
                Vector2 targetPos = boss.position;
                CameraUpdate(&myCam, targetPos, dt);
            }
            else {
                // Zoom ra xa hơn 1 chút để thấy cả 3 cục boom + cửa thoát (map rộng/cao hơn).
                myCam.zoom += (0.95f - myCam.zoom) * 2.0f * dt;
                Vector2 targetPos = { bossPlayer.position.x, bossPlayer.position.y - 70.0f };
                CameraUpdate(&myCam, targetPos, dt);
            }
        } else {
            CameraUpdate(&myCam, (Vector2){ bossPlayer.position.x, bossPlayer.position.y - 50.0f }, dt);
        }
    } else {
        CameraUpdate(&myCam, player.position, dt);
    }

    float skyAlpha = 0.0f;
    float mountainAlpha = 0.0f;
    float buildingAlpha = 0.0f;
    float fgAlpha = 0.0f;
    if (currentMapIndex == 2 && bossInitialized && boss.state == BOSS_PRE_INTRO) {
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

    if (currentMapIndex == 2 && bossInitialized) {
        // === MAP 3 (BOSS FIGHT): 2-pass rendering for correct Z-order ===
        // Pass 1.1: Sky layers
        for (int i = 0; i < gameMap.layerCount; i++) {
            TMJLayer *layer = &gameMap.layers[i];
            if (!layer->visible || strstr(layer->name, "sky") == NULL) continue;
            if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
                for (int y = 0; y < layer->height; y++) {
                    for (int x = 0; x < layer->width; x++) {
                        int gid = layer->data[y * layer->width + x];
                        if (gid == 0) continue;
                        int tsIdx = -1;
                        for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                            if (gid >= gameMap.tilesets[k].firstgid) { tsIdx = k; break; }
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
            }
            if (strcmp(layer->type, "objectgroup") == 0) {
                for (int j = 0; j < layer->objectCount; j++) {
                    TMJObject *obj = &layer->objects[j];
                    if (!obj->visible || obj->texture.id == 0) continue;
                    Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
                    if (obj->flipX) source.width = -source.width;
                    if (obj->flipY) source.height = -source.height;
                    Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
                    Vector2 origin = {0, obj->height};
                    DrawTexturePro(obj->texture, source, dest, origin, obj->rotation, Fade(WHITE, layer->opacity * obj->opacity));
                }
            }
        }
        if (boss.state == BOSS_PRE_INTRO && skyAlpha > 0.001f) {
            DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(skyAlpha * 255)});
        }

        // Pass 1.2: Mountain layers
        for (int i = 0; i < gameMap.layerCount; i++) {
            TMJLayer *layer = &gameMap.layers[i];
            if (!layer->visible || strstr(layer->name, "mountain") == NULL) continue;
            if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
                for (int y = 0; y < layer->height; y++) {
                    for (int x = 0; x < layer->width; x++) {
                        int gid = layer->data[y * layer->width + x];
                        if (gid == 0) continue;
                        int tsIdx = -1;
                        for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                            if (gid >= gameMap.tilesets[k].firstgid) { tsIdx = k; break; }
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
            }
            if (strcmp(layer->type, "objectgroup") == 0) {
                for (int j = 0; j < layer->objectCount; j++) {
                    TMJObject *obj = &layer->objects[j];
                    if (!obj->visible || obj->texture.id == 0) continue;
                    Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
                    if (obj->flipX) source.width = -source.width;
                    if (obj->flipY) source.height = -source.height;
                    Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
                    Vector2 origin = {0, obj->height};
                    DrawTexturePro(obj->texture, source, dest, origin, obj->rotation, Fade(WHITE, layer->opacity * obj->opacity));
                }
            }
        }
        if (boss.state == BOSS_PRE_INTRO && mountainAlpha > 0.001f) {
            DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(mountainAlpha * 255)});
        }

        // Pass 1.3: Building layers
        for (int i = 0; i < gameMap.layerCount; i++) {
            TMJLayer *layer = &gameMap.layers[i];
            if (!layer->visible || strstr(layer->name, "building") == NULL) continue;
            if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
                for (int y = 0; y < layer->height; y++) {
                    for (int x = 0; x < layer->width; x++) {
                        int gid = layer->data[y * layer->width + x];
                        if (gid == 0) continue;
                        int tsIdx = -1;
                        for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                            if (gid >= gameMap.tilesets[k].firstgid) { tsIdx = k; break; }
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
            }
            if (strcmp(layer->type, "objectgroup") == 0) {
                for (int j = 0; j < layer->objectCount; j++) {
                    TMJObject *obj = &layer->objects[j];
                    if (!obj->visible || obj->texture.id == 0) continue;
                    Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
                    if (obj->flipX) source.width = -source.width;
                    if (obj->flipY) source.height = -source.height;
                    Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
                    Vector2 origin = {0, obj->height};
                    DrawTexturePro(obj->texture, source, dest, origin, obj->rotation, Fade(WHITE, layer->opacity * obj->opacity));
                }
            }
        }
        if (boss.state == BOSS_PRE_INTRO && buildingAlpha > 0.001f) {
            DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(buildingAlpha * 255)});
        }

        // Draw BOSS BODY between background and foreground platforms
        DrawBossBody(&boss, texAgis, 0.0f);

        // Pass 2: Foreground layers (tiles, props — everything that is NOT background/ground/boss/agis)
        for (int i = 0; i < gameMap.layerCount; i++) {
            TMJLayer *layer = &gameMap.layers[i];
            if (!layer->visible) continue;
            // Skip background (vẽ ở pass 1)
            if (strstr(layer->name, "sky")      != NULL ||
                strstr(layer->name, "mountain") != NULL ||
                strstr(layer->name, "building") != NULL) continue;
            // Skip ground (collision only) / boss / agis (không vẽ sprite map)
            if (strstr(layer->name, "ground") != NULL) continue;
            if (strcmp(layer->name, "boss") == 0 || strcmp(layer->name, "agis") == 0) continue;

            if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
                for (int y = 0; y < layer->height; y++) {
                    for (int x = 0; x < layer->width; x++) {
                        int gid = layer->data[y * layer->width + x];
                        if (gid == 0) continue;
                        int tsIdx = -1;
                        for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                            if (gid >= gameMap.tilesets[k].firstgid) { tsIdx = k; break; }
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
            }
            if (strcmp(layer->type, "objectgroup") == 0) {
                for (int j = 0; j < layer->objectCount; j++) {
                    TMJObject *obj = &layer->objects[j];
                    if (!obj->visible || obj->texture.id == 0) continue;
                    Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
                    if (obj->flipX) source.width = -source.width;
                    if (obj->flipY) source.height = -source.height;
                    Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
                    Vector2 origin = {0, obj->height};
                    DrawTexturePro(obj->texture, source, dest, origin, obj->rotation, Fade(WHITE, layer->opacity * obj->opacity));
                }
            }
        }

        // Draw Player (under foreground overlay, so it's also darkened)
        DrawBossPlayer(&bossPlayer, texIdle, texWalk, texRun, texJump, texRunJump, texAttack, texHurt);

        // Foreground overlay
        if (boss.state == BOSS_PRE_INTRO && fgAlpha > 0.001f) {
            DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(fgAlpha * 255)});
        }

        // Glowing Beacon (above foreground overlay)
        if (boss.state == BOSS_PRE_INTRO) {
            float time = (float)GetTime();
            float pulse = sinf(time * 5.0f) * 6.0f + 16.0f;
            DrawCircleV(boss.beaconPos, pulse + 15.0f, (Color){ 255, 235, 150, 40 });
            DrawCircleV(boss.beaconPos, pulse + 5.0f, (Color){ 255, 180, 50, 100 });
            DrawCircleV(boss.beaconPos, 8.0f, (Color){ 255, 255, 200, 255 });
        }

        // === CỬA THOÁT (walk-away phase): hào quang vàng tại cửa gỗ thật trên map ===
        if (boss.state == BOSS_FAKE_DEATH && boss.walkAwayDoorActive) {
            float t = (float)GetTime();
            float pulse = (sinf(t * 4.0f) + 1.0f) * 0.5f;
            float doorX = 1080.0f, doorY = 419.0f;   // tọa độ cửa gỗ thật trên map
            // Hào quang vàng tỏa ra từ cửa gỗ
            DrawCircleV((Vector2){doorX, doorY - 30.0f}, 60.0f + pulse * 14.0f, (Color){255, 220, 120, (unsigned char)(50 + pulse*60)});
            // Mũi tên gợi ý đi sang phải
            DrawText(">>", (int)(bossPlayer.position.x + 30), (int)(bossPlayer.position.y - 90),
                30, (Color){255, 230, 120, (unsigned char)(150 + pulse*100)});
        }

        // Boss skills + boom nodes + orbs + projectiles — on top of everything
        DrawBossSkills(&boss);
        DrawBooms(&boss);
        DrawOrbs(&om);
        DrawProjectiles(&pm);

    } else {
        // === MAP 1 & 2: single pass (original logic) ===
        for (int i = 0; i < gameMap.layerCount; i++) {
            TMJLayer *layer = &gameMap.layers[i];
            if (!layer->visible) continue;
            if (strcmp(layer->type, "tilelayer") == 0 && layer->data != NULL) {
                for (int y = 0; y < layer->height; y++) {
                    for (int x = 0; x < layer->width; x++) {
                        int gid = layer->data[y * layer->width + x];
                        if (gid == 0) continue;
                        int tsIdx = -1;
                        for (int k = gameMap.tilesetCount - 1; k >= 0; k--) {
                            if (gid >= gameMap.tilesets[k].firstgid) { tsIdx = k; break; }
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
            }
            if (strcmp(layer->type, "objectgroup") == 0) {
                for (int j = 0; j < layer->objectCount; j++) {
                    TMJObject *obj = &layer->objects[j];
                    if (!obj->visible || obj->texture.id == 0) continue;
                    Rectangle source = {0, 0, (float)obj->texture.width, (float)obj->texture.height};
                    if (obj->flipX) source.width = -source.width;
                    if (obj->flipY) source.height = -source.height;
                    Rectangle dest = {obj->x + layer->offsetx, obj->y + layer->offsety, obj->width, obj->height};
                    Vector2 origin = {0, obj->height};
                    DrawTexturePro(obj->texture, source, dest, origin, obj->rotation, Fade(WHITE, layer->opacity * obj->opacity));
                }
            }
        }
        DrawPlayer(&player, texIdle, texWalk, texRun, texJump, texAttack, texRunJump, texHurt, 64, 64, 0.78f);
    }


    EndMode2D();
    EndTextureMode();

    // --- DRAW SCREEN SPACE (UI) ---
    BeginDrawing();
    ClearBackground(BLACK);
    
    Rectangle sourceRec = {0.0f, 0.0f, (float)target.texture.width,
                           (float)-target.texture.height};
    Rectangle destRec = {0, 0, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
    DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){0, 0}, 0.0f,
                   WHITE);

    if (currentMapIndex == 2 && bossInitialized) {
        if (boss.state == BOSS_FIGHTING || boss.state == BOSS_DYING ||
            boss.state == BOSS_DEFEATED || boss.state == BOSS_TRUE_ENRAGE) {
            DrawUI(bossPlayer.hp, boss.hp, boss.maxHp);

            const char *phaseText = "Phase 1";
            if (boss.phase == BOSS_PHASE_2) phaseText = "Phase 2 - Enraged";
            if (boss.phase == BOSS_PHASE_3) phaseText = "Phase 3 - Danger!";
            if (boss.phase == BOSS_PHASE_4) phaseText = "Phase 4 - FINAL FORM!";
            if (boss.state == BOSS_TRUE_ENRAGE) phaseText = "TRUE FINAL - PARRY TO KILL!";

            int textW = MeasureText(phaseText, 24);
            DrawText(phaseText, SCREEN_WIDTH / 2 - textW / 2, 70, 24, YELLOW);
        }

        // === FAKE-DEATH CUTSCENE OVERLAY (8s, 4 pha) ===
        if (boss.state == BOSS_FAKE_DEATH) {
            float t = boss.fakeDeathTimer;
            // Nhạc lịm dần ở pha 1-2 (lừa player tưởng thắng), bùng lại khi hồi sinh.
            if (currentBgTrack != BG_NONE) {
                if (t < 4.5f) {
                    float v = bgVolume * (1.0f - (t / 4.5f));
                    SetMusicVolume(bgTracks[currentBgTrack], v < 0 ? 0 : v);
                } else {
                    SetMusicVolume(bgTracks[currentBgTrack], bgVolume);
                }
            }

            if (t < 3.0f) {
                // PHA 1 COLLAPSE: tối dần + "VICTORY?" mờ hiện -> đánh lừa.
                float a = t / 3.0f;
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, (unsigned char)(a * 150)});
                if (t > 1.0f) {
                    float ta = (t - 1.0f) / 2.0f;
                    int fs = 60;
                    const char *vt = "VICTORY?";
                    int tw = MeasureText(vt, fs);
                    DrawText(vt, SCREEN_WIDTH/2 - tw/2, SCREEN_HEIGHT/2 - 40, fs,
                        (Color){230, 230, 255, (unsigned char)(ta * 200)});
                }
            } else if (t < 4.5f) {
                // PHA 2 SILENCE: tối yên tĩnh.
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){0, 0, 0, 150});
            } else if (t < 6.5f) {
                // PHA 3 REVIVAL: glitch đỏ nhấp nháy + dải nhiễu ngang.
                if (((int)(t * 30.0f)) % 2 == 0)
                    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){180, 0, 0, 70});
                for (int b = 0; b < 6; b++) {
                    int gy = (int)((sinf(t * 20.0f + b) * 0.5f + 0.5f) * SCREEN_HEIGHT);
                    DrawRectangle(0, gy, SCREEN_WIDTH, 4, (Color){255, 40, 40, 120});
                }
            } else {
                // PHA 4 DECLARE: nhuốm tím/đỏ + chữ tuyên chiến.
                float pulse = (sinf(t * 12.0f) + 1.0f) * 0.5f;
                DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                    (Color){120, 0, 60, (unsigned char)(40 + pulse * 50)});
                const char *dt2 = "IT'S NOT OVER!";
                int fs = 52; int tw = MeasureText(dt2, fs);
                DrawText(dt2, SCREEN_WIDTH/2 - tw/2, SCREEN_HEIGHT/2 - 30, fs,
                    (Color){255, 60, 120, (unsigned char)(180 + pulse*75)});
            }
        }

        if (boss.state == BOSS_INTRO) {
            float alpha = boss.introTimer / 3.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            
            int textW = MeasureText("Something approaches...", 36);
            DrawText("Something approaches...", SCREEN_WIDTH/2 - textW/2, 120, 36, 
                (Color){255, 100, 100, (unsigned char)(alpha * 200)});
            
            DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
            DrawRectangle(0, SCREEN_HEIGHT - 80, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
            
            float pct = boss.introTimer / 15.5f;
            if (pct > 1.0f) pct = 1.0f;
            DrawRectangle(40, SCREEN_HEIGHT - 50, (int)((SCREEN_WIDTH - 80) * pct), 6, (Color){255, 100, 100, 150});
        }

        if (boss.state == BOSS_ROAR) {
            float time = (float)GetTime();
            int fontSize = (int)(72.0f + sinf(time * 15.0f) * 10.0f);
            int textW = MeasureText("A G I S", fontSize);
            Color textColor = (Color){ 255, (unsigned char)(100.0f + sinf(time * 20.0f) * 100.0f), 50, 255 };
            
            DrawText("A G I S", SCREEN_WIDTH/2 - textW/2, SCREEN_HEIGHT/2 - fontSize/2, fontSize, textColor);
            DrawRectangle(0, 0, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
            DrawRectangle(0, SCREEN_HEIGHT - 80, SCREEN_WIDTH, 80, (Color){0, 0, 0, 200});
        }

        if (boss.state == BOSS_FIGHTING) {
            DrawText("A/D: Move | Space: Jump | LEFT-CLICK to PARRY the glowing orb back at AGIS!",
                40, SCREEN_HEIGHT - 50, 22, (Color){200, 200, 200, 200});
        }

        if (boss.state == BOSS_DYING) {
            int textW = MeasureText("FINAL BLOW!", 48);
            DrawText("FINAL BLOW!", SCREEN_WIDTH/2 - textW/2, SCREEN_HEIGHT/2, 48, 
                (Color){255, 255, 100, 255});
        }

        // Nhắc đi ra cửa trong giai đoạn walk-away.
        if (boss.state == BOSS_FAKE_DEATH && boss.walkAwayDoorActive) {
            const char *m = "Di ra cua thoat ben phai ->";
            int fs = 28; int tw = MeasureText(m, fs);
            float pulse = (sinf((float)GetTime() * 3.0f) + 1.0f) * 0.5f;
            DrawText(m, SCREEN_WIDTH/2 - tw/2, 130, fs,
                (Color){255, 230, 150, (unsigned char)(160 + pulse*95)});
        }

        if (bossGameState == STATE_WIN) DrawWinScreen();
        if (bossGameState == STATE_LOSE) DrawLoseScreen();
    }

    DrawFPS(10, 10);
    EndDrawing();
  }

  // --- CLEANUP ---
  if (bossInitialized) {
      UnloadMusicStream(introMusic);
      for (int i = 0; i < 4; i++) {
          UnloadMusicStream(bgTracks[i]);
      }
      UnloadSound(laughSfx);
      UnloadSound(damageSfx);
      UnloadSound(alarmSfx);
      UnloadSound(hitsSfx);
      UnloadSound(slashSfx);
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
  CloseAudioDevice();

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
