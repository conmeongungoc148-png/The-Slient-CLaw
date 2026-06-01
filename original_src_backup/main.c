Total Bytes: 41603
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
#include "camera.h"
#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Boss specific headers
#include "boss_player.h"
#include "boss.h"
#include "projectile.h"
#include "orb.h"
#include "collision.h"
#include "gamestate.h"
#include "map.h"
#include "cutscene.h"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define VIRTUAL_HEIGHT 760
#define VIRTUAL_WIDTH 1351
#define BOSS_ARENA_GROUND_Y 620.0f
#define BOSS_DEFAULT_SCALE 3.0f

static float GetBossCenterYForFeet(float feetY, float scale) {
    return feetY - ((float)BOSS_FRAME_H * scale * 0.5f);
}

static bool TextContainsNoCaseLocal(const char *text, const char *needle) {
    if (!text || !needle) return false;
    size_t needleLen = strlen(needle);
    if (needleLen == 0) return true;

    for (const char *p = text; *p; p++) {
        size_t i = 0;
        while (i < needleLen && p[i]) {
            char a = p[i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
            i++;
        }
        if (i == needleLen) return true;
    }
    return false;
}

static bool FindLargestMapObject(cute_tiled_map_t *map, const char *nameNeedle, Vector2 *outPosition, Rectangle *outRect) {
    if (!map || !nameNeedle) return false;

<truncated 32751 bytes>
OffsetY);
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

Total Bytes: 41603
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
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
            Dr
<truncated 6859 bytes>
yer->offsety, obj->width, obj->height};
                
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
