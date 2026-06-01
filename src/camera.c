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
    cam.rl.target   = (Vector2){ x, y };
    cam.rl.zoom     = 1.0f;
    cam.rl.rotation = 0.0f;

    // Trạng thái nội bộ
    cam.target      = (Vector2){ x, y };
    cam.zoom        = 1.0f;
    cam.rotation    = 0.0f;

    // Mặc định: Mượt kiểu Damped, stiffness = 8
    // Tương đương camera.smooth.damped(8) trong Lua
    cam.smoothMode  = CAM_SMOOTH_DAMPED;
    cam.smoothSpeed = 8.0f;

    cam.deadzoneEnabled = false;
    cam.boundsEnabled   = false;
    cam.shakeTimer      = 0.0f;
    cam.shakeMagnitude  = 0.0f;

    return cam;
}

// --- LookAt (tức thì) ---
// Tương đương camera:lookAt(x, y) trong Lua
void CameraLookAt(MyCamera *cam, Vector2 pos) {
    cam->target      = pos;
    cam->rl.target   = pos;
}

// --- Update (gọi mỗi frame) ---
// Đây là hàm quan trọng nhất, kết hợp mọi tính năng
void CameraUpdate(MyCamera *cam, Vector2 targetPos, float deltaTime) {
    Vector2 goal = targetPos;

    // 1. XỬ LÝ DEADZONE (Vùng chết - camera chỉ trượt khi nhân vật chạm biên)
    // Tương đương camera:lockWindow() trong Lua
    if (cam->deadzoneEnabled) {
        // Tính vị trí nhân vật trên màn hình (camera coordinates)
        Vector2 screenPos = CameraToScreen(cam, targetPos);

        float cx = cam->rl.offset.x;
        float cy = cam->rl.offset.y;
        float hw = cam->deadzone.width  / 2.0f;
        float hh = cam->deadzone.height / 2.0f;

        float dx = 0, dy = 0;

        if (screenPos.x < cx - hw) dx = screenPos.x - (cx - hw);
        if (screenPos.x > cx + hw) dx = screenPos.x - (cx + hw);
        if (screenPos.y < cy - hh) dy = screenPos.y - (cy - hh);
        if (screenPos.y > cy + hh) dy = screenPos.y - (cy + hh);

        goal.x = cam->target.x + dx;
        goal.y = cam->target.y + dy;
    }

    // 2. ÁP DỤNG SMOOTH FOLLOW
    // Tương đương camera.smooth.damped và camera.smooth.linear trong Lua
    if (cam->smoothMode == CAM_SMOOTH_DAMPED) {
        // Lerp kiểu damped: nhanh khi xa, chậm khi gần
        float t = 1.0f - expf(-cam->smoothSpeed * deltaTime);
        cam->target.x = Lerpf(cam->target.x, goal.x, t);
        cam->target.y = Lerpf(cam->target.y, goal.y, t);
    } 
    else if (cam->smoothMode == CAM_SMOOTH_LINEAR) {
        // Di chuyển với tốc độ cố định mỗi frame
        // Tương đương camera.smooth.linear(speed) trong Lua
        Vector2 diff  = { goal.x - cam->target.x, goal.y - cam->target.y };
        float   dist  = Vec2Length(diff);
        float   step  = cam->smoothSpeed * deltaTime;

        if (dist > 0 && step < dist) {
            cam->target.x += (diff.x / dist) * step;
            cam->target.y += (diff.y / dist) * step;
        } else {
            cam->target = goal; // Đã đủ gần, bám chặt luôn
        }
    } 
    else {
        // CAM_SMOOTH_NONE: Bám chặt ngay lập tức
        cam->target = goal;
    }

    // 3. GIỚI HẠN BIÊN BẢN ĐỒ (Camera Bounds Clamping)
    if (cam->boundsEnabled) {
        float halfW = cam->rl.offset.x / cam->zoom;
        float halfH = cam->rl.offset.y / cam->zoom;

        float minX = cam->bounds.x + halfW;
        float maxX = cam->bounds.x + cam->bounds.width - halfW;
        if (minX > maxX) {
            cam->target.x = cam->bounds.x + cam->bounds.width / 2.0f;
        } else {
            cam->target.x = Clampf(cam->target.x, minX, maxX);
        }

        float minY = cam->bounds.y + halfH;
        float maxY = cam->bounds.y + cam->bounds.height - halfH;
        if (minY > maxY) {
            cam->target.y = cam->bounds.y + cam->bounds.height / 2.0f;
        } else {
            cam->target.y = Clampf(cam->target.y, minY, maxY);
        }
    }

    // 4. XỬ LÝ SCREEN SHAKE (MƯỢT - không random giật từng frame)
    // Dùng dao động sin theo thời gian (đa tần số) + biên độ giảm dần.
    // Random mỗi frame gây "giật lag" rất khó chịu, nhất là khi zoom out;
    // sin theo thời gian cho rung liên tục, tự nhiên và êm.
    Vector2 shakeOffset = {0, 0};
    if (cam->shakeTimer > 0) {
        cam->shakeTimer -= deltaTime;
        if (cam->shakeTimer < 0) cam->shakeTimer = 0;
        float t = (float)GetTime();
        // Biên độ giảm dần theo thời gian còn lại của cú shake (ease-out).
        float decay = cam->shakeTimer;
        if (decay > 1.0f) decay = 1.0f;          // chuẩn hoá 0..1
        float s = cam->shakeMagnitude * decay;
        // Giới hạn biên độ tối đa để không nhảy quá xa (giảm cảm giác lag).
        if (s > 14.0f) s = 14.0f;
        shakeOffset.x = (sinf(t * 47.0f) + 0.5f * sinf(t * 23.0f)) * s;
        shakeOffset.y = (cosf(t * 41.0f) + 0.5f * cosf(t * 19.0f)) * s;
    }

    // 5. CẬP NHẬT CAMERA RAYLIB
    cam->rl.target.x = cam->target.x + shakeOffset.x;
    cam->rl.target.y = cam->target.y + shakeOffset.y;
    cam->rl.zoom     = cam->zoom;
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
        (worldPos.x - cam->target.x) * cam->zoom + cam->rl.offset.x,
        (worldPos.y - cam->target.y) * cam->zoom + cam->rl.offset.y
    };
}

// Screen → World
Vector2 CameraToWorld(MyCamera *cam, Vector2 screenPos) {
    return (Vector2){
        (screenPos.x - cam->rl.offset.x) / cam->zoom + cam->target.x,
        (screenPos.y - cam->rl.offset.y) / cam->zoom + cam->target.y
    };
}
