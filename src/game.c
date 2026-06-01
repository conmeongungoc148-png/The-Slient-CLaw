#include "game.h"
#include "cute_tiled.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Texture Cache ---
#define MAX_CACHE 128
static struct {
  char path[256];
  Texture2D texture;
} TextureCache[MAX_CACHE];
static int CacheCount = 0;

Texture2D GetCachedTexture(const char *path) {
  for (int i = 0; i < CacheCount; i++) {
    if (strcmp(TextureCache[i].path, path) == 0)
      return TextureCache[i].texture;
  }
  if (CacheCount < MAX_CACHE) {
    Texture2D tex = LoadTexture(path);
    if (tex.id != 0) {
      strncpy(TextureCache[CacheCount].path, path, 255);
      TextureCache[CacheCount].texture = tex;
      CacheCount++;
    }
    return tex;
  }
  return LoadTexture(path);
}

static void ClearCache() {
  for (int i = 0; i < CacheCount; i++)
    UnloadTexture(TextureCache[i].texture);
  CacheCount = 0;
}

// --- Helpers ---

static void FixTiledPath(char *path) {
  // Chuyển ngược thành xuôi
  char *p = path;
  while (*p) {
    if (*p == '\\')
      *p = '/';
    p++;
  }

  // Xóa "../" ở đầu nếu có (nhiều cấp nếu có, ví dụ ../../)
  while (strncmp(path, "../", 3) == 0) {
    memmove(path, path + 3, strlen(path + 3) + 1);
  }

  // Xóa "Theforest/" hoặc "Theforest-main/" nếu có trong path để đưa về root
  char *key = strstr(path, "Theforest/");
  if (key) {
    memmove(path, key + 10, strlen(key + 10) + 1);
  } else {
    key = strstr(path, "Theforest-main/");
    if (key)
      memmove(path, key + 15, strlen(key + 15) + 1);
  }

  // Prepend assets/ if not already starting with assets/ or asset_sources/
  if (strncmp(path, "asset_sources/", 14) != 0 && strncmp(path, "assets/", 7) != 0) {
    char fixed[256];
    snprintf(fixed, sizeof(fixed), "assets/%s", path);
    strncpy(path, fixed, 255);
    path[255] = '\0';
  }
}

void LoadPlayerHitbox(float *width, float *height) {
  // Default fallback values
  *width = 40.0f;
  *height = 29.25f;

  const char *hitboxPath = "boss/assets/player/playerhurt.tmj";
  cute_tiled_map_t *map = cute_tiled_load_map_from_file(hitboxPath, NULL);
  if (!map) {
    TraceLog(LOG_WARNING, "[HITBOX] Failed to load %s, using default fallback.", hitboxPath);
    return;
  }

  cute_tiled_layer_t *layer = map->layers;
  while (layer) {
    if (layer->name.ptr && strcmp(layer->name.ptr, "hitbox") == 0) {
      cute_tiled_object_t *obj = layer->objects;
      if (obj) {
        *width = obj->width;
        *height = obj->height;
        TraceLog(LOG_INFO, "[HITBOX] Loaded player hitbox from TMJ: width = %.2f, height = %.2f", *width, *height);
        break;
      }
    }
    layer = layer->next;
  }

  cute_tiled_free_map(map);
}

void InitPlayer(Player *player, Vector2 pos) {
  player->position = pos;
  player->velocity = (Vector2){0, 0};
  player->speed = (Vector2){250.0f, 0.0f};
  player->groundY = pos.y;
  player->facingRight = true;
  player->isJumping = false;
  player->isAttacking = false;
  player->isHurt = false;
  player->freezeTimer = 0.0f;
  player->currentFrame = 0;
  player->frameTimer = 0.0f;
  player->isClimbing = false;
  LoadPlayerHitbox(&player->hitboxWidth, &player->hitboxHeight);
}
void UpdatePlayer(Player *player, GameMap *map, float deltaTime) {
  const float gravity = 4000.0f;
  const float jumpForce = -1080.0f;
  float playerRadius = player->hitboxWidth / 2.0f;
  float playerHeight = player->hitboxHeight;
  float stepUpHeight = 16.0f;

  player->isRunning = false;
  player->isSprinting = false;
  float currentSpeed = player->speed.x;
  float animSpeed = 0.1f;

  if (player->freezeTimer > 0)
    player->freezeTimer -= deltaTime;

  if (IsKeyDown(KEY_LEFT_SHIFT)) {
    currentSpeed *= 1.8f;
    player->isSprinting = true;
    animSpeed = 0.07f;
  }

  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !player->isAttacking) {
    player->isAttacking = true;
    player->currentFrame = 0;
    player->frameTimer = 0.0f;
    player->freezeTimer = 0.15f;
  }

  if (player->isAttacking)
    animSpeed = 0.07f;

  float feetY = player->position.y + 64.0f;

  // --- Detect Ladder Overlapping ---
  bool nearLadder = false;
  float ladderLeft = 0.0f, ladderRight = 0.0f;
  for (int i = 0; i < map->layerCount; i++) {
    TMJLayer *layer = &map->layers[i];
    if (!layer->visible) continue;

    char lowerName[64];
    strncpy(lowerName, layer->name, 63);
    lowerName[63] = '\0';
    for (int c = 0; lowerName[c]; c++) {
      if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') lowerName[c] = lowerName[c] - 'A' + 'a';
    }

    bool isLadderLayer = (strstr(lowerName, "ladder") != NULL);
    if (strcmp(layer->type, "objectgroup") == 0) {
      for (int j = 0; j < layer->objectCount; j++) {
        TMJObject *obj = &layer->objects[j];
        char objLower[64];
        strncpy(objLower, obj->name, 63);
        objLower[63] = '\0';
        for (int c = 0; objLower[c]; c++) {
          if (objLower[c] >= 'A' && objLower[c] <= 'Z') objLower[c] = objLower[c] - 'A' + 'a';
        }
        bool isLadderObj = (strstr(objLower, "ladder") != NULL);

        if (isLadderLayer || isLadderObj) {
          float objX = obj->x + layer->offsetx;
          float objY = obj->y + layer->offsety;
          float objLeft = objX;
          float objRight = objX + obj->width;
          float objTop = objY;
          
          // Override 0-height objects (Tiled point objects representing 16px tile)
          float objHeight = obj->height;
          if (objHeight == 0.0f) objHeight = 16.0f;
          float objBottom = objY + objHeight;

          if (player->position.x >= objLeft && player->position.x <= objRight &&
              feetY >= objTop && (feetY - playerHeight) <= objBottom) {
            nearLadder = true;
            ladderLeft = objLeft;
            ladderRight = objRight;
            break;
          }
        }
      }
    }
    if (nearLadder) break;
  }

  // --- Ladder and Gravity Input Logic ---
  bool pressUp = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
  bool pressDown = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);

  if (nearLadder) {
    if (!player->isClimbing && (pressUp || pressDown)) {
      player->isClimbing = true;
      player->isJumping = false;
      player->velocity.y = 0.0f;
    }
  } else {
    player->isClimbing = false;
  }

  if (player->isClimbing) {
    player->isJumping = false;
    player->velocity.y = 0.0f;
    if (pressUp) {
      player->velocity.y = -200.0f;
    } else if (pressDown) {
      player->velocity.y = 200.0f;
    }

    if (IsKeyPressed(KEY_SPACE)) {
      player->isClimbing = false;
      player->velocity.y = jumpForce;
      player->isJumping = true;
    }
  } else {
    if (IsKeyPressed(KEY_SPACE) && !player->isJumping) {
      player->velocity.y = jumpForce;
      player->isJumping = true;
    }
    player->velocity.y += gravity * deltaTime;
  }

  float nextFeetY = feetY + player->velocity.y * deltaTime;
  float nextX = player->position.x;

  // Tính toán di chuyển ngang
  float horizontalMove = currentSpeed;
  if (player->isJumping)
    horizontalMove *= 1.2f;
  if (player->freezeTimer > 0)
    horizontalMove = 0;

  if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
    nextX += horizontalMove * deltaTime;
  else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
    nextX -= horizontalMove * deltaTime;

  if (player->isClimbing) {
    if (nextX < ladderLeft - 4.0f || nextX > ladderRight + 4.0f) {
      player->isClimbing = false;
    }
  }

  bool hitGround = false;
  float groundLevel = 100000.0f;

  if (!player->isClimbing) {
    for (int i = 0; i < map->layerCount; i++) {
      TMJLayer *layer = &map->layers[i];
      if (!layer->visible || strcmp(layer->type, "objectgroup") != 0)
        continue;

      char lowerName[64];
      strncpy(lowerName, layer->name, 63);
      lowerName[63] = '\0';
      for (int c = 0; lowerName[c]; c++) {
        if (lowerName[c] >= 'A' && lowerName[c] <= 'Z') {
          lowerName[c] = lowerName[c] - 'A' + 'a';
        }
      }

      bool isSolidLayer = (strstr(lowerName, "ground") != NULL) ||
                          (strstr(lowerName, "solid") != NULL) ||
                          (strstr(lowerName, "soild") != NULL) ||
                          (strstr(lowerName, "block") != NULL);
      bool isPortalLayer = (strstr(lowerName, "portal") != NULL);

      if (!isSolidLayer && !isPortalLayer)
        continue;

      for (int j = 0; j < layer->objectCount; j++) {
        TMJObject *obj = &layer->objects[j];
        if (obj->texture.id != 0)
          continue;

        // Skip ladders from solid collision
        char objLower[64];
        strncpy(objLower, obj->name, 63);
        objLower[63] = '\0';
        for (int c = 0; objLower[c]; c++) {
          if (objLower[c] >= 'A' && objLower[c] <= 'Z') objLower[c] = objLower[c] - 'A' + 'a';
        }
        bool isLadder = (strstr(objLower, "ladder") != NULL) || (strstr(lowerName, "ladder") != NULL);
        if (isLadder)
          continue;

        float objX = obj->x + layer->offsetx;
        float objY = obj->y + layer->offsety;

        if (isPortalLayer) {
          // Portals are disabled/commented out temporarily
          /*
          float objLeft = objX;
          float objRight = objX + obj->width;
          float objTop = objY;
          float objBottom = objY + obj->height;
          float headY = feetY - playerHeight;

          if (nextX + playerRadius > objLeft && nextX - playerRadius < objRight &&
              feetY > objTop && headY < objBottom) {
            // Trigger map change
            player->loadNextMap = true;
          }
          */
          continue;
        }

        if (obj->polygonCount > 0) {
          // --- Polygon / Slope Collision ---
          for (int k = 0; k < obj->polygonCount; k++) {
            Point p1 = obj->polygon[k];
            Point p2 = obj->polygon[(k + 1) % obj->polygonCount];
            float x1 = p1.x + objX;
            float y1 = p1.y + objY;
            float x2 = p2.x + objX;
            float y2 = p2.y + objY;
            float minX = (x1 < x2) ? x1 : x2;
            float maxX = (x1 > x2) ? x1 : x2;

            if (nextX >= minX - 1.0f && nextX <= maxX + 1.0f) {
              float slopeY = y1 + (y2 - y1) * (nextX - x1) / (x2 - x1);
              if (player->velocity.y >= 0 && feetY <= slopeY + 8.0f &&
                  nextFeetY >= slopeY - 2.0f) {
                hitGround = true;
                if (slopeY < groundLevel)
                  groundLevel = slopeY;
              }
            }
          }
        } else {
          // --- Rectangle Collision (Full Solid with Step-Up) ---
          float objLeft = objX;
          float objRight = objX + obj->width;
          float objTop = objY;
          float objBottom = objY + obj->height;
          float headY = feetY - playerHeight;

          // 1. Horizontal Collision (Walls)
          if (feetY > objTop + 2.0f && headY < objBottom - 2.0f) {
            if (nextX + playerRadius > objLeft &&
                nextX - playerRadius < objRight) {
              if (feetY > objTop && feetY <= objTop + stepUpHeight) {
                // Can step up
              } else {
                // Resolve wall collision using minimum displacement push-out
                float pushRight = objRight - (nextX - playerRadius);
                float pushLeft = (nextX + playerRadius) - objLeft;
                if (pushRight < pushLeft) {
                  nextX += pushRight;
                } else {
                  nextX -= pushLeft;
                }
              }
            }
          }

          // 2. Vertical Collision (Floor only, One-way platform)
          float groundCheckRadius = playerRadius * 0.5f; // Use a smaller radius for floor landing to allow dropping through holes
          if (nextX + groundCheckRadius > objLeft && nextX - groundCheckRadius < objRight) {
            if (player->velocity.y >= 0 && feetY <= objTop + 8.0f &&
                nextFeetY >= objTop) {
              hitGround = true;
              if (objTop < groundLevel)
                groundLevel = objTop;
            }
          }
        }
      }
    }
  }

  // Update Position
  player->position.x = nextX;
  player->isRunning = false;
  if (player->isClimbing) {
    if (pressUp || pressDown || IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
      player->isRunning = true;
    }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
      player->facingRight = true;
    } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
      player->facingRight = false;
    }
  } else {
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
      player->facingRight = true;
      player->isRunning = !player->isJumping;
    } else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
      player->facingRight = false;
      player->isRunning = !player->isJumping;
    }
  }

  if (hitGround) {
    player->position.y = groundLevel - 64.0f;
    player->velocity.y = 0;
    player->isJumping = false;
  } else {
    player->position.y = nextFeetY - 64.0f;
  }

  // --- Giới hạn biên bản đồ động ---
  float mapWidthPixels = (float)map->mapWidth * map->tileWidth;
  float mapHeightPixels = (float)map->mapHeight * map->tileHeight;

  if (player->position.x < playerRadius) {
    player->position.x = playerRadius;
  } else if (player->position.x > mapWidthPixels - playerRadius) {
    player->position.x = mapWidthPixels - playerRadius;
  }

  if (player->position.y < 0.0f) {
    player->position.y = 0.0f;
    player->velocity.y = 0.0f;
  } else if (player->position.y > mapHeightPixels - 64.0f) {
    player->position.y = mapHeightPixels - 64.0f;
    player->velocity.y = 0.0f;
    player->isJumping = false;
  }

  player->frameTimer += deltaTime;
  if (player->frameTimer >= animSpeed) {
    player->frameTimer = 0.0f;
    player->currentFrame++;
    if (player->isHurt && player->currentFrame >= 4) {
      player->isHurt = false;
      player->currentFrame = 0;
    } else if (player->isAttacking && player->currentFrame >= 8) {
      player->isAttacking = false;
      player->currentFrame = 0;
    } else if (player->isJumping) {
      if (player->isSprinting) {
        if (player->currentFrame > 2) {
          player->currentFrame = 2;
        }
      } else {
        if (player->currentFrame > 4) {
          player->currentFrame = 4;
        }
      }
    }
  }
}
void DrawPlayer(Player *player, Texture2D idle, Texture2D walk, Texture2D run,
                Texture2D jump, Texture2D attack, Texture2D runJump, Texture2D hurt, int frameW, int frameH,
                float scale) {
  float yOffset = 64.0f;
  float spacing = (frameW == 80) ? 0.0f : 16.0f;

  Texture2D currentTex = idle;
  int maxFrames = 8; // Idle has 8 frames
  int frameIdx = 0;

  if (player->isHurt) {
    currentTex = hurt;
    maxFrames = 4;
    frameIdx = player->currentFrame % maxFrames;
  } else if (player->isAttacking) {
    currentTex = attack;
    maxFrames = 8;
    frameIdx = player->currentFrame % maxFrames;
  } else if (player->isJumping) {
    if (player->isSprinting) {
      currentTex = runJump;
      maxFrames = 3;
      frameIdx = player->currentFrame % maxFrames;
    } else {
      currentTex = jump;
      maxFrames = 5;
      int animFrame = player->currentFrame % maxFrames;
      if (animFrame == 0)
        frameIdx = 0;
      else if (animFrame >= 1 && animFrame <= 3)
        frameIdx = 1;
      else
        frameIdx = 2;
    }
  } else {
    if (player->isRunning) {
      if (player->isSprinting) {
        currentTex = run;
        maxFrames = 8;
      } else {
        currentTex = walk;
        maxFrames = 8;
      }
    }
    frameIdx = player->currentFrame % maxFrames;
  }

  // Adjust spacing for custom animations:
  // RUNNING JUMP and HURT spritesheets in Tiled have "spacing": 16.
  // However, check if currentTex is runJump or hurt, or use frameW to decide.
  // The spacing for custom sheets is also 16, so keeping 16 is fine.
  float currentSpacing = spacing;
  if (currentTex.id == runJump.id || currentTex.id == hurt.id) {
    currentSpacing = 16.0f;
  }

  float frameX = (float)frameIdx * (frameW + currentSpacing);
  float drawH = (float)frameH;
  if (drawH > (float)currentTex.height)
    drawH = (float)currentTex.height;
  Rectangle source = {frameX, 0, (float)frameW, drawH};
  if (player->facingRight)
    source.width = -source.width;

  float paddingCompensation = 0.0f;
  if (frameH == 80)
    paddingCompensation = 20.0f * scale;
  else if (frameH == 64)
    paddingCompensation = 16.0f * scale;

  Rectangle dest = {player->position.x,
                    player->position.y + yOffset + paddingCompensation,
                    (float)frameW * scale, (float)frameH * scale};
  DrawTexturePro(currentTex, source, dest,
                 (Vector2){(float)frameW * scale / 2.0f, (float)frameH * scale},
                 0.0f, WHITE);
}

// --- Layer Helpers using cute_tiled ---
static int CountLayers(cute_tiled_layer_t *layer) {
  int count = 0;
  while (layer) {
    if (layer->type.ptr && strcmp(layer->type.ptr, "group") == 0) {
      count += CountLayers(layer->layers);
    } else {
      count++;
    }
    layer = layer->next;
  }
  return count;
}

static void ProcessLayers(cute_tiled_layer_t *layer, GameMap *map,
                          int *currentLayerIdx, float parentOffsetX,
                          float parentOffsetY) {
  while (layer) {
    if (layer->type.ptr && strcmp(layer->type.ptr, "group") == 0) {
      ProcessLayers(layer->layers, map, currentLayerIdx,
                    parentOffsetX + layer->offsetx,
                    parentOffsetY + layer->offsety);
    } else {
      TMJLayer *tmjLayer = &map->layers[*currentLayerIdx];
      strncpy(tmjLayer->name, layer->name.ptr ? layer->name.ptr : "", 63);
      strncpy(tmjLayer->type, layer->type.ptr ? layer->type.ptr : "", 31);
      tmjLayer->visible = layer->visible;
      tmjLayer->opacity = layer->opacity;
      tmjLayer->offsetx = parentOffsetX + layer->offsetx;
      tmjLayer->offsety = parentOffsetY + layer->offsety;

      if (strcmp(tmjLayer->type, "objectgroup") == 0) {
        // Count objects
        int objCount = 0;
        cute_tiled_object_t *obj = layer->objects;
        while (obj) {
          objCount++;
          obj = obj->next;
        }

        tmjLayer->objectCount = objCount;
        if (objCount > 0) {
          tmjLayer->objects = (TMJObject *)calloc(objCount, sizeof(TMJObject));
          obj = layer->objects;
          for (int j = 0; j < objCount && obj; j++) {
            TMJObject *tmjObj = &tmjLayer->objects[j];
            strncpy(tmjObj->name, obj->name.ptr ? obj->name.ptr : "", 63);
            tmjObj->x = obj->x;
            tmjObj->y = obj->y;
            tmjObj->width = obj->width;
            tmjObj->height = obj->height;
            tmjObj->rotation = obj->rotation;
            tmjObj->visible = obj->visible;
            tmjObj->opacity = 1.0f;

            if (obj->vert_count > 0 && obj->vert_type == 1) { // 1 is polygon
              tmjObj->polygonCount = obj->vert_count;
              tmjObj->polygon =
                  (Point *)malloc(obj->vert_count * sizeof(Point));
              for (int k = 0; k < obj->vert_count; k++) {
                tmjObj->polygon[k].x = obj->vertices[2 * k];
                tmjObj->polygon[k].y = obj->vertices[2 * k + 1];
              }
            }

            if (obj->gid != 0) {
              unsigned int rawGid = (unsigned int)obj->gid;
              tmjObj->flipX = (rawGid & CUTE_TILED_FLIPPED_HORIZONTALLY_FLAG);
              tmjObj->flipY = (rawGid & CUTE_TILED_FLIPPED_VERTICALLY_FLAG);
              int gid = cute_tiled_unset_flags(rawGid);

              // Find matching tileset texture
              for (int k = map->tilesetCount - 1; k >= 0; k--) {
                if (gid >= map->tilesets[k].firstgid) {
                  if (map->tilesets[k].texture.id != 0) {
                    tmjObj->texture = map->tilesets[k].texture;
                  }
                  break;
                }
              }
            }
            obj = obj->next;
          }
        }
      } else if (strcmp(tmjLayer->type, "tilelayer") == 0) {
        tmjLayer->width = layer->width;
        tmjLayer->height = layer->height;
        if (layer->data_count > 0) {
          tmjLayer->data = (int *)malloc(layer->data_count * sizeof(int));
          for (int j = 0; j < layer->data_count; j++) {
            tmjLayer->data[j] = layer->data[j];
          }
        }
      }
      (*currentLayerIdx)++;
    }
    layer = layer->next;
  }
}

GameMap LoadMapData(const char *filename) {
  GameMap map = {0};
  cute_tiled_map_t *tiled_map = cute_tiled_load_map_from_file(filename, NULL);
  if (!tiled_map) {
    TraceLog(LOG_ERROR, "[MAP] Failed to load map file: %s", filename);
    return map;
  }

  map.mapWidth = tiled_map->width;
  map.mapHeight = tiled_map->height;
  map.tileWidth = tiled_map->tilewidth;
  map.tileHeight = tiled_map->tileheight;

  // --- Load Tilesets ---
  int tsCount = 0;
  cute_tiled_tileset_t *ts = tiled_map->tilesets;
  while (ts) {
    tsCount++;
    ts = ts->next;
  }
  map.tilesetCount = tsCount;
  if (tsCount > 0) {
    map.tilesets = (TMJTileset *)calloc(tsCount, sizeof(TMJTileset));
    ts = tiled_map->tilesets;
    for (int i = 0; i < tsCount && ts; i++) {
      map.tilesets[i].firstgid = ts->firstgid;
      map.tilesets[i].tileWidth = ts->tilewidth;
      map.tilesets[i].tileHeight = ts->tileheight;
      map.tilesets[i].spacing = ts->spacing;
      map.tilesets[i].margin = ts->margin;
      map.tilesets[i].columns = ts->columns;

      if (ts->image.ptr && strlen(ts->image.ptr) > 0) {
        strncpy(map.tilesets[i].imagePath, ts->image.ptr, 255);
        char fullPath[256];
        if (strstr(map.tilesets[i].imagePath, "/") ||
            strstr(map.tilesets[i].imagePath, "\\")) {
          strncpy(fullPath, map.tilesets[i].imagePath, 255);
        } else {
          snprintf(fullPath, 255, "assets/%s", map.tilesets[i].imagePath);
        }
        FixTiledPath(fullPath);
        map.tilesets[i].texture = GetCachedTexture(fullPath);
      } else {
        cute_tiled_tile_descriptor_t *tile = ts->tiles;
        if (tile && tile->image.ptr && strlen(tile->image.ptr) > 0) {
          strncpy(map.tilesets[i].imagePath, tile->image.ptr, 255);
          char fullPath[256];
          if (strstr(map.tilesets[i].imagePath, "/") ||
              strstr(map.tilesets[i].imagePath, "\\")) {
            strncpy(fullPath, map.tilesets[i].imagePath, 255);
          } else {
            snprintf(fullPath, 255, "assets/%s", map.tilesets[i].imagePath);
          }
          FixTiledPath(fullPath);
          map.tilesets[i].texture = GetCachedTexture(fullPath);
        }
      }
      ts = ts->next;
    }
  }

  // --- Load Layers ---
  map.layerCount = CountLayers(tiled_map->layers);
  if (map.layerCount > 0) {
    map.layers = (TMJLayer *)calloc(map.layerCount, sizeof(TMJLayer));
    int currentIdx = 0;
    ProcessLayers(tiled_map->layers, &map, &currentIdx, 0.0f, 0.0f);
  }

  cute_tiled_free_map(tiled_map);
  return map;
}

void UnloadMapData(GameMap *map) {
  ClearCache();
  for (int i = 0; i < map->layerCount; i++) {
    if (map->layers[i].objects) {
      for (int j = 0; j < map->layers[i].objectCount; j++) {
        if (map->layers[i].objects[j].polygon)
          free(map->layers[i].objects[j].polygon);
      }
      free(map->layers[i].objects);
    }
    if (map->layers[i].data)
      free(map->layers[i].data);
  }
  if (map->layers)
    free(map->layers);
  if (map->tilesets)
    free(map->tilesets);
}
