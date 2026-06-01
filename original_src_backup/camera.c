Total Bytes: 7009
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
// camera.c - Triển khai hệ thống Camera cho The Forest
// Port từ camera.lua (HUMP library) sang C + Raylib
// Nguyên tắc: Không bao giờ thay đổi tỷ lệ gốc của "kịch bản" (900x760)

#include "camera.h"
#include "raylib.h"
#include <math.h>

// ============================================================
// Hàm nội bộ (private helpers)
// ============================================================

// Nội suy tuyến tính (Linear Interpolation) - Tương đương Lua smooth.damped
static float Lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

// Clamp một giá trị trong khoảng [min, max]
static float Clampf(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// Chiều dài vector
static float Vec2Length(Vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

// ============================================================
// Triển khai API
// ============================================================

// --- Khởi tạo ---
// Tương đương hàm new(x, y, zoom, rot, smoother) trong camera.lua
MyCamera CameraNew(float x, float y, float screenW, float screenH) {
    MyCamera cam = {0};

    // Cài đặt Camera2D của Raylib
    // offset = tâm màn hình để camera luôn nhìn về giữa
    cam.rl.offset   = (Vector2){ screenW / 2.0f, screenH / 2.0f };
    cam.rl.target   = (Vector2){ x, 
<truncated 4374 bytes>
 cam->zoom;
    cam->rl.rotation = cam->rotation;
}

// --- Deadzone ---
void CameraSetDeadzone(MyCamera *cam, float w, float h) {
    cam->deadzoneEnabled  = true;
    cam->deadzone.width   = w;
    cam->deadzone.height  = h;
}

// --- Bounds (Giới hạn biên) ---
void CameraSetBounds(MyCamera *cam, float x, float y, float w, float h) {
    cam->boundsEnabled   = true;
    cam->bounds.x        = x;
    cam->bounds.y        = y;
    cam->bounds.width    = w;
    cam->bounds.height   = h;
}

// --- Smooth Modes ---
void CameraSetSmoothNone(MyCamera *cam) {
    cam->smoothMode = CAM_SMOOTH_NONE;
}

void CameraSetSmoothDamped(MyCamera *cam, float stiffness) {
    cam->smoothMode  = CAM_SMOOTH_DAMPED;
    cam->smoothSpeed = stiffness;
}

void CameraSetSmoothLinear(MyCamera *cam, float speed) {
    cam->smoothMode  = CAM_SMOOTH_LINEAR;
    cam->smoothSpeed = speed;
}

// --- Screen Shake ---
void CameraShake(MyCamera *cam, float duration, float magnitude) {
    cam->shakeTimer     = duration;
    cam->shakeMagnitude = magnitude;
}

// --- Chuyển đổi tọa độ ---
// Tương đương camera:cameraCoords() và camera:worldCoords() trong Lua

// World → Screen
Vector2 CameraToScreen(MyCamera *cam, Vector2 worldPos) {
    return (Vector2){
        (worldPos.x - cam->rl.target.x) * cam->zoom + cam->rl.offset.x,
        (worldPos.y - cam->rl.target.y) * cam->zoom + cam->rl.offset.y
    };
}

// Screen → World
Vector2 CameraToWorld(MyCamera *cam, Vector2 screenPos) {
    return (Vector2){
        (screenPos.x - cam->rl.offset.x) / cam->zoom + cam->rl.target.x,
        (screenPos.y - cam->rl.offset.y) / cam->zoom + cam->rl.target.y
    };
}
