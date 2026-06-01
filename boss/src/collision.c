#include "collision.h"
#include <math.h>

bool CheckCollision(Rectangle a, Rectangle b) {
    return (a.x < b.x + b.width &&
            a.x + a.width > b.x &&
            a.y < b.y + b.height &&
            a.y + a.height > b.y);
}

bool CheckPointInRect(Vector2 point, Rectangle rect) {
    return (point.x >= rect.x && 
            point.x <= rect.x + rect.width &&
            point.y >= rect.y && 
            point.y <= rect.y + rect.height);
}

float Distance(Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

Vector2 NormalizeVec(Vector2 v) {
    float length = sqrtf(v.x * v.x + v.y * v.y);
    if (length > 0) {
        return (Vector2){v.x / length, v.y / length};
    }
    return (Vector2){0, 0};
}