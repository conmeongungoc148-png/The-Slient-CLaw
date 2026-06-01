#ifndef COLLISION_H
#define COLLISION_H

#include "raylib.h"
#include <stdbool.h>

// AABB Collision Detection
bool CheckCollision(Rectangle a, Rectangle b);
bool CheckPointInRect(Vector2 point, Rectangle rect);

// Tính khoảng cách giữa 2 điểm
float Distance(Vector2 a, Vector2 b);

// Normalize vector (chuyển về độ dài = 1)
Vector2 NormalizeVec(Vector2 v);

#endif // COLLISION_H