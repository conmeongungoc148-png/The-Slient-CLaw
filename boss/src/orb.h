#ifndef ORB_H
#define ORB_H

#include "raylib.h"
#include "cute_tiled.h"
#include <stdbool.h>

#define MAX_ORBS 5

typedef enum {
    ORB_FALLING,        // Boss vừa nhả ra, đang bay chậm về phía player (chưa parry được)
    ORB_ARMED,          // Đang sáng vàng - PARRY WINDOW mở, player phải đánh trúng
    ORB_DUD,            // Lỡ parry: orb tắt lửa, rơi xuống đất rồi nằm lại 1 lúc trước khi mất
    ORB_READY,          // (legacy) đã chạm sàn, chờ player bắt
    ORB_RETURNING,      // Player đã parry, đang bay về boss
    ORB_INACTIVE
} OrbState;

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Rectangle hitbox;
    OrbState state;
    int damage;
    float radius;
    float pulseTimer;   // Hiệu ứng nhấp nháy
    float armedTimer;   // Đếm ngược thời gian còn lại của parry window (ARMED)
    float lifeTimer;    // Tổng thời gian sống, để fizzle nếu không ai parry
    Vector2 targetPos;  // Vị trí mục tiêu bay tới khi returning
} Orb;

typedef struct {
    Orb orbs[MAX_ORBS];
} OrbManager;

void InitOrbManager(OrbManager *om);
void OrbSetArenaMap(cute_tiled_map_t *map, float offsetY, float fallbackGroundY);
void SpawnOrb(OrbManager *om, Vector2 spawnPos);
void SpawnOrbToward(OrbManager *om, Vector2 spawnPos, Vector2 playerPos);
void UpdateOrbs(OrbManager *om, Vector2 playerPos, Vector2 bossPos, float groundY, float dt);
void TryCatchOrb(OrbManager *om, Rectangle playerHurtBox, Vector2 targetPos);
void SpawnParryOrb(OrbManager *om, Vector2 spawnPos, Vector2 playerPos);
void DrawOrbs(OrbManager *om);

#endif // ORB_H
