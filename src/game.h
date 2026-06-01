#ifndef GAME_H
#define GAME_H

#include "raylib.h"

// --- Tiled Structures ---
typedef struct {
    float x, y;
} Point;

typedef struct {
    char name[64];
    float x, y, width, height, rotation;
    bool visible, flipX, flipY;
    Texture2D texture;
    float opacity;
    Point *polygon;
    int polygonCount;
} TMJObject;

typedef struct {
    char name[64];
    char type[32];
    bool visible;
    float opacity;
    TMJObject *objects;
    int objectCount;
    int *data;      // For tile layers
    int width, height;
    float offsetx, offsety;
} TMJLayer;

typedef struct {
    int firstgid;
    char imagePath[256];
    Texture2D texture;
    int tileWidth, tileHeight;
    int spacing, margin;
    int columns;
} TMJTileset;

typedef struct {
    int mapWidth, mapHeight, tileWidth, tileHeight;
    TMJLayer *layers;
    int layerCount;
    TMJTileset *tilesets;
    int tilesetCount;
} GameMap;

// --- Player Structure ---
typedef struct {
    Vector2 position;
    Vector2 velocity; // Để xử lý trọng lực và quán tính
    Vector2 speed;
    int currentFrame;
    float frameTimer;
    float groundY;    // Mốc mặt đất để va chạm
    bool isRunning;
    bool isSprinting;
    bool isJumping;
    bool isAttacking;
    bool isHurt;       // Trạng thái bị thương (khóa tạm thời)
    bool facingRight;
    float freezeTimer; // Thời gian khựng khi tấn công
    bool loadNextMap;  // Flag để báo hiệu chuyển map
    bool isClimbing;   // Trạng thái trèo thang
    float hitboxWidth;
    float hitboxHeight;
} Player;

// --- API ---
GameMap LoadMapData(const char *filename);
void UnloadMapData(GameMap *map);
Texture2D GetCachedTexture(const char* path);

void InitPlayer(Player *player, Vector2 pos);
void LoadPlayerHitbox(float *width, float *height);
void UpdatePlayer(Player *player, GameMap *map, float deltaTime);
void DrawPlayer(Player *player, Texture2D idle, Texture2D walk, Texture2D run, Texture2D jump, Texture2D attack, Texture2D runJump, Texture2D hurt, int frameW, int frameH, float scale);

#endif
