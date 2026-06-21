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
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

#define GROUND_Y 620.0f
#define BOSS_X (SCREEN_WIDTH / 2)
#define BOSS_Y 280.0f

static float cameraShake = 0.0f;
static float mapOffsetY = 0.0f;

extern int gPreIntroSlowWalk;

Font gGameFont;


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

static void ResetGame(BossPlayer *player, Boss *boss, ProjectileManager *pm, OrbManager *om) {
    InitBossPlayer(player, (Vector2){200, GROUND_Y}, GROUND_Y);
    InitBoss(boss, (Vector2){BOSS_X, 900.0f + mapOffsetY}, (Vector2){BOSS_X, BOSS_Y + mapOffsetY});
    InitProjectileManager(pm);
    InitOrbManager(om);
    cameraShake = 0.0f;
}

static void DrawFallbackArena(void) {
    // Background gradient
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 
        (Color){30, 10, 50, 255},
        (Color){10, 5, 20, 255});

    DrawRectangle(0, (int)GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)GROUND_Y, 
        (Color){60, 40, 80, 255});
    DrawRectangle(0, (int)(GROUND_Y - 4), SCREEN_WIDTH, 4, (Color){150, 80, 180, 255});

    DrawRectangle(0, 0, 20, SCREEN_HEIGHT, (Color){40, 20, 60, 255});
    DrawRectangle(SCREEN_WIDTH - 20, 0, 20, SCREEN_HEIGHT, (Color){40, 20, 60, 255});

    for (int x = 0; x < SCREEN_WIDTH; x += 80) {
        DrawLine(x, (int)GROUND_Y, x, SCREEN_HEIGHT, (Color){80, 50, 100, 100});
    }
}

int main(void) {
    srand((unsigned int)time(NULL));

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Silent Claw");
    InitAudioDevice();
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    // Load intro music (chính xác 15.5s)
    Music introMusic = LoadMusicStream("assets/audio/music/start.ogg");
    introMusic.looping = false;
    bool introMusicStarted = false;

    // Load background music tracks
    bgTracks[BG_PHRASE12] = LoadMusicStream("assets/audio/music/background/phrase1and2.ogg");
    bgTracks[BG_PHRASE3] = LoadMusicStream("assets/audio/music/background/phrase3.ogg");
    bgTracks[BG_PHRASE4] = LoadMusicStream("assets/audio/music/background/phrase4.ogg");
    bgTracks[BG_ENDING] = LoadMusicStream("assets/audio/music/background/ending.ogg");
    
    // Set looping (ending không loop)
    bgTracks[BG_PHRASE12].looping = true;
    bgTracks[BG_PHRASE3].looping = true;
    bgTracks[BG_PHRASE4].looping = true;
    bgTracks[BG_ENDING].looping = false;

    // Load laugh SFX (chạy khi hiện chữ AGIS / ROAR)
    Sound laughSfx = LoadSound("assets/audio/sfx/laugh.ogg");
    bool laughPlayed = false;

    // Load damage SFX (khi boss bị sát thương)
    Sound damageSfx = LoadSound("assets/audio/sfx/damage.ogg");

    // Load thêm SFX
    Sound alarmSfx = LoadSound("assets/audio/sfx/alarm.ogg");   // Warning (claw/hazard/laser)
    Sound hitsSfx = LoadSound("assets/audio/sfx/hits.ogg");      // Player bắt orb
    Sound slashSfx = LoadSound("assets/audio/sfx/slash.ogg");    // Claw slash

    Texture2D texAgis    = LoadTexture("assets/boss/sprites/agis.png");
    gGameFont = LoadFontEx("assets/other/MedievalSharp-Regular.ttf", 96, NULL, 0);
    SetTextureFilter(gGameFont.texture, TEXTURE_FILTER_BILINEAR);
    Texture2D texCatIdle = LoadTexture("assets/boss/sprites/cat/IDLE.png");
    Texture2D texCatWalk = LoadTexture("assets/boss/sprites/cat/WALK.png");
    Texture2D texCatRun  = LoadTexture("assets/boss/sprites/cat/RUN.png");
    Texture2D texCatJump = LoadTexture("assets/boss/sprites/cat/JUMP.png");
    Texture2D texCatRunJump = LoadTexture("assets/boss/sprites/cat/RUNNING JUMP.png");
    Texture2D texCatAttack = LoadTexture("assets/boss/sprites/cat/ATTACK 1.png");
    Texture2D texCatHurt = LoadTexture("assets/boss/sprites/cat/HURT.png");
    Texture2D texForestTiles = LoadTexture("../asset_sources/forest_tiles/Final/Tiles.png");
    cute_tiled_map_t *forestMap = MapLoad("assets/boss map 1v1.tmj");
    if (forestMap) {
        mapOffsetY = GROUND_Y - MapGetGroundY(forestMap);
        BossSetArenaMap(forestMap, mapOffsetY, GROUND_Y);
        OrbSetArenaMap(forestMap, mapOffsetY, GROUND_Y);
    }

    BossPlayer player;
    Boss boss;
    ProjectileManager pm;
    OrbManager om;

    ResetGame(&player, &boss, &pm, &om);

    // === Camera2D với zoom ===
    Camera2D camera = { 0 };
    camera.target = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
    camera.offset = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    GameState gameState = STATE_PLAYING;

    // Track previous warning states để detect transition (chỉ play alarm 1 lần/event)
    bool prevClawActive = false;
    bool prevLaserActive = false;
    int prevHazardCount = 0;
    bool clawSlashPlayed = false;
    bool prevRainActive = false;
    // Track statue states to detect activation transitions
    StatueAnimState prevStatueAnimState[MAX_MAP_STATUES] = {0};

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // === UPDATE MUSIC STREAMS ===
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
        
        // Bắt đầu bg music khi vào FIGHTING lần đầu
        if (boss.state == BOSS_FIGHTING && currentBgTrack == BG_NONE && !fadingOut) {
            BgTrack startTrack = GetTrackForPhase(boss.phase);
            currentBgTrack = startTrack;
            PlayMusicStream(bgTracks[currentBgTrack]);
            bgVolume = 0.0f;
            bgVolumeTarget = 1.0f;
            SetMusicVolume(bgTracks[currentBgTrack], bgVolume);
            lastPhase = boss.phase;
        }

        // Detect phase change → switch music với fade
        if (boss.state == BOSS_FIGHTING && currentBgTrack != BG_NONE) {
            if (boss.phase != lastPhase) {
                BgTrack desiredTrack = GetTrackForPhase(boss.phase);
                if (desiredTrack != currentBgTrack && !fadingOut) {
                    SwitchBgTrack(desiredTrack);
                }
                lastPhase = boss.phase;
            }
        }

        // Detect DYING → switch to ending music
        if (boss.state == BOSS_DYING && !endingTriggered) {
            if (currentBgTrack != BG_ENDING && !fadingOut) {
                SwitchBgTrack(BG_ENDING);
            }
            endingTriggered = true;
        }

        // Volume fade logic
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

        // Handle fade-out complete: switch to pending track
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

        // Ending music finished → trigger boss defeated
        if (currentBgTrack == BG_ENDING && !fadingOut && bgVolume > 0.5f) {
            float played = GetMusicTimePlayed(bgTracks[BG_ENDING]);
            float total = GetMusicTimeLength(bgTracks[BG_ENDING]);
            if (total > 0 && played >= total - 0.2f) {
                boss.defeated = true;
                boss.state = BOSS_DEFEATED;
            }
        }

        // === UPDATE ===
        if (gameState == STATE_PLAYING) {
            gPreIntroSlowWalk = (boss.state == BOSS_PRE_INTRO);
            UpdateBoss(&boss, player.position, &pm, &om, dt, &cameraShake);

            // Decay camera shake over time in standalone main.c
            bool bossSetsShakeEveryFrame = (boss.state == BOSS_INTRO) || (boss.state == BOSS_ROAR) || (boss.state == BOSS_DYING);
            if (!bossSetsShakeEveryFrame) {
                if (cameraShake > 0.0f) {
                    cameraShake -= dt * 40.0f; // decay
                    if (cameraShake < 0.0f) cameraShake = 0.0f;
                }
            }

            // === Camera Zoom & Target ===
            if (boss.state == BOSS_PRE_INTRO) {
                camera.zoom += (2.2f - camera.zoom) * 2.0f * dt;
                float halfViewW = (SCREEN_WIDTH / 2.0f) / camera.zoom;
                float halfViewH = (SCREEN_HEIGHT / 2.0f) / camera.zoom;
                float targetX = player.position.x;
                float targetY = player.position.y - 50.0f;
                if (targetX < halfViewW) targetX = halfViewW;
                if (targetX > SCREEN_WIDTH - halfViewW) targetX = SCREEN_WIDTH - halfViewW;
                if (targetY < halfViewH) targetY = halfViewH;
                if (targetY > SCREEN_HEIGHT - halfViewH) targetY = SCREEN_HEIGHT - halfViewH;
                
                camera.target.x += (targetX - camera.target.x) * 2.0f * dt;
                camera.target.y += (targetY - camera.target.y) * 2.0f * dt;
            }
            else if (boss.state == BOSS_INTRO) {
                float progress = boss.introTimer / 15.5f;
                if (progress > 1.0f) progress = 1.0f;
                
                float targetZoom = 2.2f - 0.9f * progress; // Smoothly zoom out from 2.2f to 1.3f
                camera.zoom += (targetZoom - camera.zoom) * 2.0f * dt;
                
                Vector2 targetPos = boss.position;
                float halfViewW = (SCREEN_WIDTH / 2.0f) / camera.zoom;
                float halfViewH = (SCREEN_HEIGHT / 2.0f) / camera.zoom;
                float targetX = targetPos.x;
                float targetY = targetPos.y;
                if (targetX < halfViewW) targetX = halfViewW;
                if (targetX > SCREEN_WIDTH - halfViewW) targetX = SCREEN_WIDTH - halfViewW;
                if (targetY < halfViewH) targetY = halfViewH;
                if (targetY > SCREEN_HEIGHT - halfViewH) targetY = SCREEN_HEIGHT - halfViewH;
                
                camera.target.x += (targetX - camera.target.x) * 3.0f * dt;
                camera.target.y += (targetY - camera.target.y) * 3.0f * dt;
            }
            else if (boss.state == BOSS_ROAR) {
                camera.zoom += (1.8f - camera.zoom) * 3.0f * dt;
                Vector2 targetPos = boss.position;
                float halfViewW = (SCREEN_WIDTH / 2.0f) / camera.zoom;
                float halfViewH = (SCREEN_HEIGHT / 2.0f) / camera.zoom;
                float targetX = targetPos.x;
                float targetY = targetPos.y;
                if (targetX < halfViewW) targetX = halfViewW;
                if (targetX > SCREEN_WIDTH - halfViewW) targetX = SCREEN_WIDTH - halfViewW;
                if (targetY < halfViewH) targetY = halfViewH;
                if (targetY > SCREEN_HEIGHT - halfViewH) targetY = SCREEN_HEIGHT - halfViewH;
                
                camera.target.x += (targetX - camera.target.x) * 3.0f * dt;
                camera.target.y += (targetY - camera.target.y) * 3.0f * dt;
            }
            else {
                camera.zoom += (1.3f - camera.zoom) * 2.0f * dt;
                float halfViewW = (SCREEN_WIDTH / 2.0f) / camera.zoom;
                float halfViewH = (SCREEN_HEIGHT / 2.0f) / camera.zoom;
                float targetX = player.position.x;
                float targetY = player.position.y - 50.0f;
                if (targetX < halfViewW) targetX = halfViewW;
                if (targetX > SCREEN_WIDTH - halfViewW) targetX = SCREEN_WIDTH - halfViewW;
                if (targetY < halfViewH) targetY = halfViewH;
                if (targetY > SCREEN_HEIGHT - halfViewH) targetY = SCREEN_HEIGHT - halfViewH;
                
                camera.target.x += (targetX - camera.target.x) * 2.0f * dt;
                camera.target.y += (targetY - camera.target.y) * 2.0f * dt;
            }

            // SHAKE MƯỢT: dao động sin theo thời gian thay vì random từng frame.
            // Random mỗi frame gây giật lag khó chịu; sin cho rung êm, tự nhiên.
            {
                float st = (float)GetTime();
                float s = cameraShake;
                if (s > 14.0f) s = 14.0f;   // giới hạn biên độ để bớt cảm giác lag
                float ox = (sinf(st * 47.0f) + 0.5f * sinf(st * 23.0f)) * s;
                float oy = (cosf(st * 41.0f) + 0.5f * cosf(st * 19.0f)) * s;
                camera.offset.x = SCREEN_WIDTH/2.0f + ox;
                camera.offset.y = SCREEN_HEIGHT/2.0f + oy;
            }

            // Check win/lose
            if (boss.defeated && boss.deathTimer >= 4.0f) gameState = STATE_WIN;
            if (player.hp <= 0) gameState = STATE_LOSE;

            // Player update logic (with lock check)
            bool isPlayerLocked = (boss.state == BOSS_PRE_INTRO && boss.preIntroTriggered) || (boss.state == BOSS_INTRO) || (boss.state == BOSS_ROAR);

            if (isPlayerLocked) {
                player.velocity = (Vector2){0, 0};
                player.state = PSTATE_IDLE;
                player.isJumping = false;
                player.isRunning = false;
                player.isSprinting = false;
                player.isAttacking = false;
                player.isHurt = false;
                player.position.x = boss.beaconPos.x;
                player.frameTimer += dt;
                if (player.frameTimer >= 0.12f) {
                    player.frameTimer = 0.0f;
                    player.currentFrame = (player.currentFrame + 1) % 10;
                }
                player.hurtBox.x = player.position.x - 10;
                player.hurtBox.y = player.position.y - 30;
            } else {
                if (boss.state == BOSS_PRE_INTRO || boss.state == BOSS_FIGHTING) {
                    UpdateBossPlayerOnMap(&player, forestMap, mapOffsetY, GROUND_Y, dt);
                }
            }

            if (boss.state == BOSS_FIGHTING) {
                UpdateProjectiles(&pm, dt);
                UpdateOrbs(&om, player.position, boss.position, GROUND_Y, dt);

                // === ALARM SOUND khi xuất hiện warning ===
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

                // Bắt orb → hits.ogg
                {
                    bool wasReady[MAX_ORBS];
                    for (int i = 0; i < MAX_ORBS; i++) {
                        wasReady[i] = (om.orbs[i].state == ORB_READY);
                    }
                    Vector2 orbTarget = boss.position;
                    int bi = BoomNearestActive(&boss, player.position);
                    if (bi >= 0) {
                        orbTarget = boss.booms[bi].position;
                    }
                    TryCatchOrb(&om, player.hurtBox, orbTarget);
                    for (int i = 0; i < MAX_ORBS; i++) {
                        if (wasReady[i] && om.orbs[i].state == ORB_RETURNING) {
                            PlaySound(hitsSfx);
                        }
                    }
                }

                // Crystal hit registration when player attacks
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !player.isHurt && !player.isAttacking) {
                    Rectangle attackBox;
                    float range = 60.0f;
                    float height = 50.0f;
                    if (player.facingRight) {
                        attackBox = (Rectangle){ player.position.x, player.position.y - 45.0f, range, height };
                    } else {
                        attackBox = (Rectangle){ player.position.x - range, player.position.y - 45.0f, range, height };
                    }
                    BossCheckStatueHits(&boss, attackBox, hitsSfx, slashSfx, &om);
                }

                // === Statue activation SFX tracking ===
                for (int i = 0; i < boss.mapStatueCount; i++) {
                    StatueAnimState cur = boss.mapStatues[i].animState;
                    StatueAnimState prev = prevStatueAnimState[i];
                    // Statue just started activating → play alarm as dramatic warning
                    if (prev == STATUE_INACTIVE && cur == STATUE_ACTIVATING) {
                        PlaySound(alarmSfx);
                    }
                    // Activation complete → stop alarm
                    if (prev == STATUE_ACTIVATING && cur == STATUE_ACTIVE) {
                        StopSound(alarmSfx);
                    }
                    // Statue just started shattering → play shatter SFX (hitsSfx as impact)
                    if (prev == STATUE_ACTIVE && cur == STATUE_SHATTERING) {
                        PlaySound(slashSfx);
                    }
                    prevStatueAnimState[i] = cur;
                }

                for (int i = 0; i < MAX_PROJECTILES; i++) {
                    if (!pm.projectiles[i].active) continue;
                    if (CheckCollision(pm.projectiles[i].hitbox, player.hurtBox)) {
                        PlayerTakeDamage(&player, pm.projectiles[i].damage);
                        pm.projectiles[i].active = false;
                        pm.count--;
                    }
                }

                for (int i = 0; i < MAX_ORBS; i++) {
                    if (om.orbs[i].state != ORB_RETURNING) continue;
                    int bi = BoomNearestActive(&boss, om.orbs[i].position);
                    if (bi >= 0) {
                        float dx = boss.booms[bi].position.x - om.orbs[i].position.x;
                        float dy = boss.booms[bi].position.y - om.orbs[i].position.y;
                        if (dx*dx + dy*dy < 70.0f*70.0f) {
                            BoomHit(&boss, bi);
                            PlaySound(damageSfx);
                            om.orbs[i].state = ORB_INACTIVE;
                            boss.orbActive = false;
                        }
                    }
                }

                if (boss.laserActive && boss.laserChargeTime <= 0) {
                    if (CheckPlayerInLaser(player.position, boss.laserStart, boss.laserEnd, 20.0f)) {
                        PlayerTakeDamage(&player, 1);
                    }
                }

                if (boss.slamActive && boss.shockwaveRadius > 50.0f) {
                    if (CheckPlayerInShockwave(player.position, boss.slamPos, boss.shockwaveRadius)) {
                        PlayerTakeDamage(&player, 2);
                    }
                }

                if (boss.clawActive && boss.clawWarningTime <= 0 && boss.clawDuration > 0) {
                    if (CheckPlayerInClawZone(player.position, boss.clawZone)) {
                        PlayerTakeDamage(&player, 2);
                    }
                }

                for (int i = 0; i < boss.hazardCount; i++) {
                    if (!boss.hazardActive[i]) continue;
                    Rectangle hazardRect = {
                        boss.hazardPositions[i].x - 10.8f,
                        boss.hazardPositions[i].y - 48.2f,
                        21.6f, 53.52f
                    };
                    if (CheckCollision(hazardRect, player.hurtBox)) {
                        PlayerTakeDamage(&player, 1);
                    }
                }

                if (player.position.x < 30) player.position.x = 30;
                if (player.position.x > SCREEN_WIDTH - 30) player.position.x = SCREEN_WIDTH - 30;
            } else {
                StopSound(alarmSfx);
            }

        } else {
            if (IsKeyPressed(KEY_R)) {
                ResetGame(&player, &boss, &pm, &om);
                gameState = STATE_PLAYING;
                camera.zoom = 1.0f;
                camera.target = (Vector2){ SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
                
                // Reset music system
                StopMusicStream(introMusic);
                if (currentBgTrack != BG_NONE) {
                    StopMusicStream(bgTracks[currentBgTrack]);
                }
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
                prevRainActive = false;
                for (int i = 0; i < MAX_MAP_STATUES; i++) prevStatueAnimState[i] = STATUE_INACTIVE;
                StopSound(alarmSfx);
            }
        }
        float skyAlpha = 0.0f;
        float mountainAlpha = 0.0f;
        float buildingAlpha = 0.0f;
        float fgAlpha = 0.0f;
        if (boss.state == BOSS_PRE_INTRO) {
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

        // === DRAW ===
        BeginTextureMode(target);
        ClearBackground(BLACK);

        BeginMode2D(camera);
        if (forestMap) {
            // Pass 1.1: Draw Sky layers
            cute_tiled_layer_t* layer = forestMap->layers;
            while (layer) {
                if (layer->visible && strstr(layer->name.ptr, "sky") != NULL) {
                    MapDrawLayerEx(forestMap, layer->name.ptr, 0.0f, mapOffsetY, texForestTiles);
                }
                layer = layer->next;
            }
            if (boss.state == BOSS_PRE_INTRO && skyAlpha > 0.001f) {
                DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(skyAlpha * 255)});
            }

            // Pass 1.2: Draw Mountain layers
            layer = forestMap->layers;
            while (layer) {
                if (layer->visible && strstr(layer->name.ptr, "mountain") != NULL) {
                    MapDrawLayerEx(forestMap, layer->name.ptr, 0.0f, mapOffsetY, texForestTiles);
                }
                layer = layer->next;
            }
            if (boss.state == BOSS_PRE_INTRO && mountainAlpha > 0.001f) {
                DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(mountainAlpha * 255)});
            }

            // Pass 1.3: Draw Building layers
            layer = forestMap->layers;
            while (layer) {
                if (layer->visible && strstr(layer->name.ptr, "building") != NULL) {
                    MapDrawLayerEx(forestMap, layer->name.ptr, 0.0f, mapOffsetY, texForestTiles);
                }
                layer = layer->next;
            }
            if (boss.state == BOSS_PRE_INTRO && buildingAlpha > 0.001f) {
                DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(buildingAlpha * 255)});
            }

            // Draw Boss Body
            DrawBossBody(&boss, texAgis, 0.0f);

            // Pass 2: Foreground layers (everything else except background, ground, boss, agis)
            layer = forestMap->layers;
            while (layer) {
                if (layer->visible) {
                    if (strstr(layer->name.ptr, "sky") == NULL &&
                        strstr(layer->name.ptr, "mountain") == NULL &&
                        strstr(layer->name.ptr, "building") == NULL &&
                        strstr(layer->name.ptr, "ground") == NULL &&
                        strcmp(layer->name.ptr, "boss") != 0 &&
                        strcmp(layer->name.ptr, "agis") != 0) {
                        MapDrawLayerEx(forestMap, layer->name.ptr, 0.0f, mapOffsetY, texForestTiles);
                    }
                }
                layer = layer->next;
            }
            
            // Draw overlay shadow
            if (boss.state != BOSS_PRE_INTRO) {
                DrawRectangle(0, (int)GROUND_Y, SCREEN_WIDTH, SCREEN_HEIGHT - (int)GROUND_Y,
                    (Color){8, 10, 16, 70});
            }
        } else {
            DrawFallbackArena();
            if (boss.state == BOSS_PRE_INTRO && skyAlpha > 0.001f) {
                DrawRectangle(-2000, -2000, 6000, 6000, (Color){0, 0, 0, (unsigned char)(skyAlpha * 255)});
            }
            DrawBossBody(&boss, texAgis, 0.0f);
        }

        // Draw Player (under foreground overlay, so it's also darkened)
        DrawBossPlayer(&player, texCatIdle, texCatWalk, texCatRun, texCatJump, texCatRunJump, texCatAttack, texCatHurt);

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



        // Draw skills, orbs, projectiles on top of everything
        DrawBossSkills(&boss);
        DrawOrbs(&om);
        DrawProjectiles(&pm);
        EndMode2D();

        if (boss.state == BOSS_FIGHTING || boss.state == BOSS_DYING || boss.state == BOSS_DEFEATED || boss.state == BOSS_OUTRO) {
            DrawUI(player.hp, boss.hp, boss.maxHp, boss.state == BOSS_OUTRO || boss.state == BOSS_DYING || boss.state == BOSS_DEFEATED);

            const char *phaseText = "Phase 1";
            if (boss.phase == BOSS_PHASE_2) phaseText = "Phase 2 - Enraged";
            if (boss.phase == BOSS_PHASE_3) phaseText = "Phase 3 - Danger!";
            if (boss.phase == BOSS_PHASE_4) phaseText = "Phase 4 - FINAL!";
            DrawText(phaseText, SCREEN_WIDTH / 2 - 80, 65, 18, YELLOW);
        }

        if (boss.state == BOSS_INTRO) {
            float alpha = boss.introTimer / 3.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            DrawText("Something approaches...", SCREEN_WIDTH/2 - 150, 80, 24, 
                (Color){255, 100, 100, (unsigned char)(alpha * 200)});
            
            DrawRectangle(0, 0, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
            DrawRectangle(0, SCREEN_HEIGHT - 60, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
            
            float pct = boss.introTimer / 15.5f;
            if (pct > 1.0f) pct = 1.0f;
            DrawRectangle(20, SCREEN_HEIGHT - 40, (int)(200 * pct), 4, (Color){255, 100, 100, 150});
        }

        if (boss.state == BOSS_ROAR) {
            float time = (float)GetTime();
            int fontSize = (int)(48.0f + sinf(time * 15.0f) * 8.0f);
            int textW = MeasureText("A G I S", fontSize);
            Color textColor = (Color){ 255, (unsigned char)(100.0f + sinf(time * 20.0f) * 100.0f), 50, 255 };
            
            DrawText("A G I S", SCREEN_WIDTH/2 - textW/2, SCREEN_HEIGHT/2 - fontSize/2, fontSize, textColor);
            DrawRectangle(0, 0, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
            DrawRectangle(0, SCREEN_HEIGHT - 60, SCREEN_WIDTH, 60, (Color){0, 0, 0, 200});
        }

        if (boss.state == BOSS_FIGHTING) {
            DrawText("A/D: Move  |  Space: Jump  |  Catch yellow orbs to damage boss!", 
                20, SCREEN_HEIGHT - 30, 16, (Color){200, 200, 200, 200});
        }

        if (boss.state == BOSS_DYING) {
            DrawText("FINAL BLOW!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 36, 
                (Color){255, 255, 100, 255});
        }



        if (gameState == STATE_WIN) DrawWinScreen();
        if (gameState == STATE_LOSE) DrawLoseScreen();

        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle sourceRec = { 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height };
        Rectangle destRec = { 0, 0, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT };
        DrawTexturePro(target.texture, sourceRec, destRec, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadTexture(texAgis);
    UnloadFont(gGameFont);
    UnloadTexture(texCatIdle);
    UnloadTexture(texCatWalk);
    UnloadTexture(texCatRun);
    UnloadTexture(texCatJump);
    UnloadTexture(texCatRunJump);
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
    UnloadRenderTexture(target);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}


