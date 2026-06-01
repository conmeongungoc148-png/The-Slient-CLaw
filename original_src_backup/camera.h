Total Bytes: 2455
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
// camera.h - Hệ thống Camera cho The Forest
// Cảm hứng từ camera.lua (HUMP library by Matthias Richter)
// Được "port" sang C + Raylib

#ifndef CAMERA_H
#define CAMERA_H

#include "raylib.h"
#include <stdbool.h>

// --- Cấu hình Camera ---
#define CAM_SMOOTH_NONE   0  // Bám chặt nhân vật
#define CAM_SMOOTH_DAMPED 1  // Mượt mà kiểu lò xo (damped)
#define CAM_SMOOTH_LINEAR 2  // Mượt mà tốc độ cố định

// --- Struct Camera tùy chỉnh ---
// Bọc lại Camera2D của Raylib và thêm các tính năng nâng cao
typedef struct {
    Camera2D   rl;            // Camera2D gốc của Raylib (để vẽ)

    // Vị trí mục tiêu mong muốn (target đang bám đến)
    Vector2    target;
    float      zoom;
    float      rotation;

    // Smooth mode & tốc độ
    int        smoothMode;
    float      smoothSpeed;   // Stiffness cho DAMPED, px/s cho LINEAR

    // Deadzone (vùng chết - camera đứng yên khi nhân vật trong vùng này)
    bool       deadzoneEnabled;
    Rectangle  deadzone;      // Tọa độ màn hình, tính từ tâm

    // Giới hạn biên bản đồ
    bool       boundsEnabled;
    Rectangle  bounds;        // Phạm vi bản đồ trong world space

    // Screen shake
    float      shakeTimer;
    float      shakeMagnitude;
} MyCamera;

// --- API Functions ---

// Khởi tạo camera ở vị trí (x, y) với mode mặc định
MyCamera CameraNew(float x, float y, float screenW, float screenH);

// Đặt mục tiêu cho camera bám theo (tức thì)
void CameraLookAt(MyCamera *cam, Vector2 pos);

// Cập nhật camera mỗi frame (bao gồm smooth, deadzone, bounds, shake)
void CameraUpdate(MyCamera *cam, Vector2 targetPos, float deltaTime);

// Bật/tắt Deadzone và thiết lập kích thước vùng chết
void CameraSetDeadzone(MyCamera *cam, float w, float h);

// Bật giới hạn biên bản đồ
void CameraSetBounds(MyCamera *cam, float x, float y, float w, float h);

// Chế độ Smooth
void CameraSetSmoothNone(MyCamera *cam);
void CameraSetSmoothDamped(MyCamera *cam, float stiffness);
void CameraSetSmoothLinear(MyCamera *cam, float speed);

// Hiệu ứng rung màn hình
void CameraShake(MyCamera *cam, float duration, float magnitude);

// Chuyển đổi tọa độ
Vector2 CameraToWorld(MyCamera *cam, Vector2 screenPos);
Vector2 CameraToScreen(MyCamera *cam, Vector2 worldPos);

#endif // CAMERA_H
