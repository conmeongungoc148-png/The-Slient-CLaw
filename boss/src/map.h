#ifndef MAP_H
#define MAP_H

#include "cute_tiled.h"
#include "raylib.h"

// --- API Quản lý Bản đồ ---

// Nạp dữ liệu bản đồ từ file .tmj
cute_tiled_map_t* MapLoad(const char* filename);

// Giải phóng bản đồ và bộ nhớ đệm hình ảnh
void MapUnload(cute_tiled_map_t* map);

// Vẽ một layer cụ thể của bản đồ
void MapDrawLayer(cute_tiled_map_t* map, const char* layerName, float offsetX, Texture2D defaultTileset);
void MapDrawLayerEx(cute_tiled_map_t* map, const char* layerName, float offsetX, float offsetY, Texture2D defaultTileset);

// Lấy tọa độ Y của mặt đất từ layer "ground" (nếu có)
float MapGetGroundY(cute_tiled_map_t* map);
float MapGetFloorYAtX(cute_tiled_map_t* map, float worldX, float currentY, float nextY, float offsetY, float fallbackY);
bool MapFindObject(cute_tiled_map_t* map, const char* nameNeedle, Vector2* outPosition, Rectangle* outRect);

// Xử lý đường dẫn hình ảnh từ Tiled sang Raylib
void MapFixPath(char* path);

#endif // MAP_H
