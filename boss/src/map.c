#define CUTE_TILED_IMPLEMENTATION
#include "map.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// --- Texture Cache cho Bản đồ ---
#define MAX_MAP_CACHE 128
static struct {
  char path[256];
  Texture2D texture;
} MapTextureCache[MAX_MAP_CACHE];
static int MapCacheCount = 0;

static Texture2D GetCachedTexture(const char *path) {
  for (int i = 0; i < MapCacheCount; i++) {
    if (strcmp(MapTextureCache[i].path, path) == 0)
      return MapTextureCache[i].texture;
  }
  if (MapCacheCount < MAX_MAP_CACHE) {
    Texture2D tex = LoadTexture(path);
    if (tex.id != 0) {
      strncpy(MapTextureCache[MapCacheCount].path, path, 255);
      MapTextureCache[MapCacheCount].texture = tex;
      MapCacheCount++;
    }
    return tex;
  }
  return LoadTexture(path);
}

static void ClearMapCache() {
  for (int i = 0; i < MapCacheCount; i++)
    UnloadTexture(MapTextureCache[i].texture);
  MapCacheCount = 0;
}

// --- Helpers ---
void MapFixPath(char *path) {
  char *p = path;
  while ((p = strstr(p, "\\/"))) {
    memmove(p, p + 1, strlen(p));
  }

  for (p = path; *p; p++) {
    if (*p == '\\')
      *p = '/';
  }

  if (strncmp(path, "../assets/", 10) == 0 ||
      strncmp(path, "../asset_sources/", 17) == 0 ||
      strncmp(path, "assets/", 7) == 0) {
    return;
  }

  while (strncmp(path, "../", 3) == 0) {
    memmove(path, path + 3, strlen(path + 3) + 1);
  }

  if (strncmp(path, "asset_sources/", 14) == 0) {
    char fixed[256];
    snprintf(fixed, sizeof(fixed), "../%s", path);
    strncpy(path, fixed, 255);
    path[255] = '\0';
  } else if (strncmp(path, "assets/", 7) != 0) {
    char fixed[256];
    snprintf(fixed, sizeof(fixed), "assets/%s", path);
    strncpy(path, fixed, 255);
    path[255] = '\0';
  }
}

static cute_tiled_tileset_t *FindTileset(cute_tiled_map_t *map, int gid) {
  cute_tiled_tileset_t *ts = map->tilesets;
  cute_tiled_tileset_t *best = NULL;
  while (ts) {
    if (gid >= ts->firstgid) {
      if (!best || ts->firstgid > best->firstgid)
        best = ts;
    }
    ts = ts->next;
  }
  return best;
}

// --- API Implementation ---

cute_tiled_map_t *MapLoad(const char *filename) {
  cute_tiled_map_t *map = cute_tiled_load_map_from_file(filename, NULL);
  if (!map)
    printf("Error loading map %s: %s\n", filename, cute_tiled_error_reason);
  return map;
}

void MapUnload(cute_tiled_map_t *map) {
  ClearMapCache();
  cute_tiled_free_map(map);
}

void MapDrawLayer(cute_tiled_map_t *map, const char *layerName, float offsetX,
                  Texture2D defaultTileset) {
  MapDrawLayerEx(map, layerName, offsetX, 0.0f, defaultTileset);
}

void MapDrawLayerEx(cute_tiled_map_t *map, const char *layerName, float offsetX,
                    float offsetY, Texture2D defaultTileset) {
  if (!map)
    return;
  cute_tiled_layer_t *layer = map->layers;
  while (layer) {
    if (strcmp(layer->name.ptr, layerName) == 0 && layer->visible) {
      // 1. TILE LAYER
      if (strcmp(layer->type.ptr, "tilelayer") == 0) {
        for (int i = 0; i < layer->data_count; i++) {
          int rawGid = layer->data[i];
          int gid = cute_tiled_unset_flags(rawGid);
          if (gid == 0)
            continue;

          cute_tiled_tileset_t *ts = FindTileset(map, gid);
          if (!ts)
            continue;

          int tileId = gid - ts->firstgid;
          int tsCols = ts->columns;
          int tw = ts->tilewidth;
          int th = ts->tileheight;

          Texture2D currentTex = defaultTileset;
          if (ts->image.ptr && strlen(ts->image.ptr) > 0) {
            char tsPath[256];
            strncpy(tsPath, ts->image.ptr, 255);
            MapFixPath(tsPath);
            currentTex = GetCachedTexture(tsPath);
          }

          Rectangle source = {(float)(tileId % tsCols) * tw,
                              (float)(tileId / tsCols) * th, (float)tw,
                              (float)th};
          int x = i % layer->width;
          int y = i / layer->width;
          Vector2 pos = {(float)x * map->tilewidth + offsetX,
                         (float)y * map->tileheight + offsetY};

          DrawTextureRec(currentTex, source, pos, Fade(WHITE, layer->opacity));
        }
      }
      // 2. OBJECT LAYER
      else if (strcmp(layer->type.ptr, "objectgroup") == 0) {
        cute_tiled_object_t *obj = layer->objects;
        while (obj) {
          if (obj->visible && obj->gid != 0) {
            int rawGid = obj->gid;
            int gid = cute_tiled_unset_flags(rawGid);
            cute_tiled_tileset_t *ts = FindTileset(map, gid);
            if (ts) {
              cute_tiled_tile_descriptor_t *td = ts->tiles;
              while (td) {
                if (td->tile_index == (gid - ts->firstgid)) {
                  char path[256];
                  strncpy(path, td->image.ptr, 255);
                  MapFixPath(path);
                  Texture2D tex = GetCachedTexture(path);
                  if (tex.id != 0) {
                    Rectangle source = {0, 0, (float)tex.width,
                                        (float)tex.height};
                    if (rawGid & CUTE_TILED_FLIPPED_HORIZONTALLY_FLAG)
                      source.width = -source.width;
                    Rectangle dest = {obj->x + offsetX, obj->y + offsetY,
                                      (float)obj->width, (float)obj->height};
                    DrawTexturePro(tex, source, dest,
                                   (Vector2){0, (float)obj->height},
                                   obj->rotation, Fade(WHITE, layer->opacity));
                  }
                  break;
                }
                td = td->next;
              }
            }
          }
          obj = obj->next;
        }
      }
      break;
    }
    layer = layer->next;
  }
}

float MapGetGroundY(cute_tiled_map_t *map) {
  if (!map)
    return 0;
  cute_tiled_layer_t *l = map->layers;
  while (l) {
    if (strcmp(l->name.ptr, "ground") == 0 && l->objects) {
      return l->objects->y;
    }
    l = l->next;
  }
  return 642.0f; // Default fallback
}

static bool TextContainsNoCase(const char *haystack, const char *needle) {
  if (!haystack || !needle || !*needle)
    return false;
  size_t needleLen = strlen(needle);
  for (const char *h = haystack; *h; h++) {
    size_t i = 0;
    while (i < needleLen && h[i] &&
           tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
      i++;
    }
    if (i == needleLen)
      return true;
  }
  return false;
}

float MapGetFloorYAtX(cute_tiled_map_t *map, float worldX, float currentY,
                      float nextY, float offsetY, float fallbackY) {
  if (!map)
    return fallbackY;

  float bestY = fallbackY;
  cute_tiled_layer_t *layer = map->layers;
  while (layer) {
    bool isGroundLayer = TextContainsNoCase(layer->name.ptr, "ground") ||
                         TextContainsNoCase(layer->name.ptr, "solid") ||
                         TextContainsNoCase(layer->name.ptr, "soild") ||
                         TextContainsNoCase(layer->name.ptr, "platform");
    if (layer->visible && layer->type.ptr &&
        strcmp(layer->type.ptr, "objectgroup") == 0 && isGroundLayer) {
      cute_tiled_object_t *obj = layer->objects;
      while (obj) {
        float left = obj->x + layer->offsetx;
        float right = left + obj->width;
        float top = obj->y + layer->offsety + offsetY;

        if (worldX >= left && worldX <= right && currentY <= top + 8.0f &&
            nextY >= top - 2.0f && top < bestY) {
          bestY = top;
        }
        obj = obj->next;
      }
    }
    layer = layer->next;
  }

  return bestY;
}

bool MapFindObject(cute_tiled_map_t *map, const char *nameNeedle,
                   Vector2 *outPosition, Rectangle *outRect) {
  if (!map || !nameNeedle)
    return false;

  cute_tiled_layer_t *layer = map->layers;
  while (layer) {
    if (layer->type.ptr && strcmp(layer->type.ptr, "objectgroup") == 0) {
      cute_tiled_object_t *obj = layer->objects;
      while (obj) {
        const char *objName = obj->name.ptr ? obj->name.ptr : "";
        const char *objType = obj->type.ptr ? obj->type.ptr : "";
        const char *layerName = layer->name.ptr ? layer->name.ptr : "";
        if (TextContainsNoCase(objName, nameNeedle) ||
            TextContainsNoCase(objType, nameNeedle) ||
            (TextContainsNoCase(layerName, nameNeedle) && obj->gid == 0)) {
          if (outPosition) {
            outPosition->x = obj->x + layer->offsetx;
            outPosition->y = obj->y + layer->offsety;
          }
          if (outRect) {
            *outRect =
                (Rectangle){obj->x + layer->offsetx, obj->y + layer->offsety,
                            obj->width, obj->height};
          }
          return true;
        }
        obj = obj->next;
      }
    }
    layer = layer->next;
  }
  return false;
}
