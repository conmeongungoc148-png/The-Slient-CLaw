#include "orb.h"
#include "collision.h"
#include "map.h"
#include "boss_assets.h"
#include <math.h>
#include <stdio.h>
#include <stddef.h>

// Lazy-loaded fire sprite for orb falling effect (shipfire06, 5 frames 58x72)
static Texture2D orbFireTex = {0};
static bool orbFireLoaded = false;
#define ORB_FIRE_FRAMES 5
#define ORB_FIRE_FW 58
#define ORB_FIRE_FH 72

#define ORB_RADIUS 18.0f  // Nhỏ hơn (từ 20 -> 18)
#define ORB_RETURN_SPEED 600.0f
#define ORB_DAMAGE 18      // 12 -> 18 (parry chuẩn ~4-5 lần là hạ boss)
#define GRAVITY 800.0f

// === BASEBALL PARRY TUNING ===
#define ORB_TRAVEL_SPEED   95.0f   // bay chậm hơn (140 -> 95) cho dễ canh, đỡ "gấp"
#define ORB_ARC_GRAVITY    180.0f  // trọng lực nhẹ -> orb bay theo vòng cung tự nhiên
#define ORB_ARM_DELAY      0.55f   // sau khi spawn bao lâu thì mở parry window
#define ORB_PARRY_WINDOW   0.70f   // parry window DÀI hơn (0.32 -> 0.70) cho dễ trúng
#define ORB_FADE_TIME      0.35f   // lỡ parry: orb mờ dần tại chỗ rồi tan (KHÔNG rơi xuống đất)
#define ORB_MAX_LIFE       7.0f    // an toàn: tự huỷ nếu sống quá lâu

static cute_tiled_map_t *arenaMap = NULL;
static float arenaMapOffsetY = 0.0f;
static float arenaFallbackGroundY = 620.0f;

static float GetSurfaceYAtX(float x) {
    float ground = arenaFallbackGroundY;
    if (arenaMap) {
        ground = MapGetFloorYAtX(arenaMap, x, -10000.0f, 10000.0f, arenaMapOffsetY, arenaFallbackGroundY);
    }
    return ground;                                           // Ground
}

void OrbSetArenaMap(cute_tiled_map_t *map, float offsetY, float fallbackGroundY) {
    arenaMap = map;
    arenaMapOffsetY = offsetY;
    arenaFallbackGroundY = fallbackGroundY;
}

void InitOrbManager(OrbManager *om) {
    for (int i = 0; i < MAX_ORBS; i++) {
        om->orbs[i].state = ORB_INACTIVE;
        om->orbs[i].radius = ORB_RADIUS;
        om->orbs[i].damage = ORB_DAMAGE;
        om->orbs[i].pulseTimer = 0.0f;
        om->orbs[i].armedTimer = 0.0f;
        om->orbs[i].lifeTimer = 0.0f;
    }
}

void SpawnParryOrb(OrbManager *om, Vector2 spawnPos, Vector2 playerPos) {
    for (int i = 0; i < MAX_ORBS; i++) {
        if (om->orbs[i].state == ORB_INACTIVE) {
            Orb *o = &om->orbs[i];
            o->position = spawnPos;
            Vector2 dir = { playerPos.x - spawnPos.x, playerPos.y - spawnPos.y };
            float dist = sqrtf(dir.x*dir.x + dir.y*dir.y);
            if (dist > 1.0f) { dir.x /= dist; dir.y /= dist; }
            else { dir = (Vector2){0, 1}; }
            
            // Bay theo vòng cung parabol: bắn chéo lên trên hướng về phía player
            o->velocity = (Vector2){ dir.x * 200.0f, -250.0f };
            o->state = ORB_FALLING;
            o->damage = ORB_DAMAGE;
            o->radius = ORB_RADIUS;
            o->pulseTimer = 0.0f;
            o->armedTimer = 0.0f;
            o->lifeTimer = 0.0f;
            o->targetPos = (Vector2){0, 0};
            o->hitbox = (Rectangle){ spawnPos.x - ORB_RADIUS, spawnPos.y - ORB_RADIUS, ORB_RADIUS*2, ORB_RADIUS*2 };
            return;
        }
    }
}

void SpawnOrb(OrbManager *om, Vector2 spawnPos) {
    // Tìm slot trống
    for (int i = 0; i < MAX_ORBS; i++) {
        if (om->orbs[i].state == ORB_INACTIVE) {
            om->orbs[i].position = spawnPos;
            // Velocity ban đầu = 0 (sẽ rơi thẳng xuống với gravity)
            om->orbs[i].velocity = (Vector2){0, 0};
            om->orbs[i].state = ORB_FALLING;
            om->orbs[i].hitbox = (Rectangle){
                spawnPos.x - ORB_RADIUS,
                spawnPos.y - ORB_RADIUS,
                ORB_RADIUS * 2,
                ORB_RADIUS * 2
            };
            return;
        }
    }
}

void SpawnOrbToward(OrbManager *om, Vector2 spawnPos, Vector2 playerPos) {
    // Tìm slot trống
    for (int i = 0; i < MAX_ORBS; i++) {
        if (om->orbs[i].state == ORB_INACTIVE) {
            om->orbs[i].position = spawnPos;
            // Bắn theo hướng player với vận tốc ban đầu (parabolic arc)
            Vector2 dir = {
                playerPos.x - spawnPos.x,
                playerPos.y - spawnPos.y
            };
            float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (dist > 1.0f) {
                dir.x /= dist;
                dir.y /= dist;
            }
            // Velocity hướng về player, sau đó gravity sẽ kéo xuống
            float speed = 180.0f;
            om->orbs[i].velocity = (Vector2){dir.x * speed, dir.y * speed * 0.5f};
            om->orbs[i].state = ORB_FALLING;
            om->orbs[i].hitbox = (Rectangle){
                spawnPos.x - ORB_RADIUS,
                spawnPos.y - ORB_RADIUS,
                ORB_RADIUS * 2,
                ORB_RADIUS * 2
            };
            return;
        }
    }
}

void UpdateOrbs(OrbManager *om, Vector2 playerPos, Vector2 bossPos, float groundY, float dt) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *orb = &om->orbs[i];
        if (orb->state == ORB_INACTIVE) continue;

        orb->pulseTimer += dt;
        orb->lifeTimer += dt;

        switch (orb->state) {
            case ORB_FALLING:
                // Rơi theo trọng lực
                orb->velocity.y += GRAVITY * dt;
                orb->position.x += orb->velocity.x * dt;
                orb->position.y += orb->velocity.y * dt;

                // Nếu chạm đất -> nằm trên sàn sàn
                {
                    float ground = GetSurfaceYAtX(orb->position.x);
                    if (orb->position.y >= ground - 5.0f) {
                        orb->position.y = ground - 5.0f;
                        orb->velocity = (Vector2){0, 0};
                        orb->state = ORB_READY;
                        orb->lifeTimer = 0.0f; // reset life timer để đếm thời gian tồn tại trên đất
                    }
                }
                
                // An toàn: nếu bay ra ngoài map thì huỷ
                if (orb->position.x < -50 || orb->position.x > 1330 || orb->position.y > 770) {
                    orb->state = ORB_INACTIVE;
                }
                break;

            case ORB_ARMED:
                orb->state = ORB_INACTIVE;
                break;

            case ORB_DUD:
                orb->state = ORB_INACTIVE;
                break;

            case ORB_READY:
                // Nằm yên trên sàn, tự hủy sau 15s nếu player bỏ qua
                if (orb->lifeTimer >= 15.0f) {
                    orb->state = ORB_INACTIVE;
                }
                break;

            case ORB_RETURNING:
                // Bay về mục tiêu được lưu (cục boom gần nhất hoặc boss)
                {
                    Vector2 dir = {
                        orb->targetPos.x - orb->position.x,
                        orb->targetPos.y - orb->position.y
                    };
                    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if (len > 1.0f) {
                        dir.x /= len;
                        dir.y /= len;
                    } else {
                        dir = (Vector2){0, 0};
                    }
                    orb->velocity = (Vector2){ dir.x * ORB_RETURN_SPEED, dir.y * ORB_RETURN_SPEED };
                    orb->position.x += orb->velocity.x * dt;
                    orb->position.y += orb->velocity.y * dt;

                    // Kiểm tra đã chạm target chưa
                    if (len < 40.0f) {
                        orb->state = ORB_INACTIVE; // Đã hit, hủy orb
                    }
                }
                break;

            case ORB_INACTIVE:
                break;
        }

        // Cập nhật hitbox
        orb->hitbox.x = orb->position.x - orb->radius;
        orb->hitbox.y = orb->position.y - orb->radius;
    }
}

void TryCatchOrb(OrbManager *om, Rectangle playerHurtBox, Vector2 targetPos) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *orb = &om->orbs[i];
        // Player bắt orb khi nó READY (nằm trên sàn)
        if (orb->state != ORB_READY) continue;

        // Check collision với player
        if (CheckCollision(orb->hitbox, playerHurtBox)) {
            orb->state = ORB_RETURNING;
            orb->velocity = (Vector2){0, 0}; // Reset velocity
            orb->targetPos = targetPos;
        }
    }
}

void DrawOrbs(OrbManager *om) {
    for (int i = 0; i < MAX_ORBS; i++) {
        Orb *orb = &om->orbs[i];
        if (orb->state == ORB_INACTIVE) continue;

        // Lazy load the fire texture
        if (!orbFireLoaded) {
            orbFireTex = LoadTexture(GetBossAssetPath("assets/effects/shipfire/orb_fire_sheet.png"));
            SetTextureFilter(orbFireTex, TEXTURE_FILTER_POINT);
            orbFireLoaded = true;
        }

        if (orb->state == ORB_FALLING) {
            // Orb is falling: draw yellow fire sheet rotated in direction of movement
            if (orbFireTex.id > 0) {
                int fireFrame = ((int)(orb->pulseTimer * 12.0f)) % ORB_FIRE_FRAMES;
                Rectangle fireSrc = { (float)(fireFrame * ORB_FIRE_FW), 0, (float)ORB_FIRE_FW, (float)ORB_FIRE_FH };
                
                float sizeW = 45.0f;
                float sizeH = 60.0f;
                Rectangle fireDst = { orb->position.x, orb->position.y, sizeW, sizeH };
                Vector2 origin = { sizeW / 2.0f, sizeH / 2.0f }; // Center pivot for rotation
                
                float angle = atan2f(orb->velocity.y, orb->velocity.x) * (180.0f / 3.14159265f);
                float rotation = angle + 90.0f; // Align UP-pointing flame to velocity direction
                if (orb->velocity.x == 0.0f && orb->velocity.y == 0.0f) {
                    rotation = 180.0f; // Default falling down
                }
                
                DrawTexturePro(orbFireTex, fireSrc, fireDst, origin, rotation, WHITE);
            } else {
                // Fallback circle
                float pulseScale = 1.0f + sinf(orb->pulseTimer * 8.0f) * 0.1f;
                DrawCircleV(orb->position, orb->radius * pulseScale + 4, (Color){255, 220, 100, 60});
                DrawCircleV(orb->position, orb->radius * pulseScale, (Color){255, 220, 100, 255});
                DrawCircleV(orb->position, orb->radius * pulseScale * 0.6f, (Color){255, 255, 255, 180});
            }
        } 
        else if (orb->state == ORB_ARMED) {
            // PARRY WINDOW: orb sáng vàng rực, vòng glow lớn nhấp nháy nhanh -> báo player ĐÁNH NGAY.
            float blink = (sinf(orb->pulseTimer * 30.0f) + 1.0f) * 0.5f;
            float r = orb->radius * (1.3f + blink * 0.3f);
            DrawCircleV(orb->position, r + 14.0f, (Color){255, 230, 60, (unsigned char)(80 + blink*100)});
            DrawCircleV(orb->position, r + 6.0f,  (Color){255, 245, 120, (unsigned char)(150 + blink*80)});
            DrawCircleV(orb->position, r,         (Color){255, 255, 200, 255});
            DrawCircleLines((int)orb->position.x, (int)orb->position.y, r + 18.0f,
                (Color){255, 255, 255, (unsigned char)(120 + blink*120)});
            // Dấu "!" nhỏ phía trên báo hiệu parry
            DrawText("!", (int)orb->position.x - 4, (int)(orb->position.y - r - 34.0f), 24,
                (Color){255, 255, 80, (unsigned char)(150 + blink*100)});
        }
        else if (orb->state == ORB_DUD) {
            // Lỡ parry: orb mờ dần tại chỗ (fade-out), nở nhẹ ra rồi tan — sạch, không rơi sàn.
            float fade = orb->armedTimer / ORB_FADE_TIME;
            if (fade < 0.0f) fade = 0.0f;
            if (fade > 1.0f) fade = 1.0f;
            float grow = 1.0f + (1.0f - fade) * 0.6f;  // nở to dần khi tan
            unsigned char a = (unsigned char)(200 * fade);
            DrawCircleV(orb->position, orb->radius * grow, (Color){255, 220, 120, (unsigned char)(a*0.5f)});
            DrawCircleV(orb->position, orb->radius * 0.6f * grow, (Color){255, 240, 180, a});
        }
        else if (orb->state == ORB_READY) {
            // Sitting on floor waiting to be caught: draw pulsing yellow circle
            float pulseScale = 1.0f + sinf(orb->pulseTimer * 5.0f) * 0.25f;
            Color color = (Color){255, 255, 0, 255};
            DrawCircleV(orb->position, orb->radius * pulseScale + 4, (Color){color.r, color.g, color.b, 60});
            DrawCircleV(orb->position, orb->radius * pulseScale, color);
            DrawCircleV(orb->position, orb->radius * pulseScale * 0.6f, (Color){255, 255, 255, 180});
        } 
        else if (orb->state == ORB_RETURNING) {
            // Flying back to boss: draw rotated cyan fire texture and energy trail
            if (orbFireTex.id > 0) {
                // Draw energy trail circles behind the fire orb
                Vector2 velDir = { 0 };
                float speed = sqrtf(orb->velocity.x * orb->velocity.x + orb->velocity.y * orb->velocity.y);
                if (speed > 1.0f) {
                    velDir = (Vector2){ orb->velocity.x / speed, orb->velocity.y / speed };
                }
                for (int j = 1; j <= 3; j++) {
                    float alpha = 1.0f - (float)j * 0.3f;
                    Vector2 trailPos = {
                        orb->position.x - velDir.x * j * 12.0f,
                        orb->position.y - velDir.y * j * 12.0f
                    };
                    DrawCircleV(trailPos, orb->radius * (1.0f - (float)j * 0.2f), 
                        (Color){0, 255, 255, (unsigned char)(alpha * 120)});
                }

                int fireFrame = ((int)(orb->pulseTimer * 12.0f)) % ORB_FIRE_FRAMES;
                Rectangle fireSrc = { (float)(fireFrame * ORB_FIRE_FW), 0, (float)ORB_FIRE_FW, (float)ORB_FIRE_FH };
                
                float sizeW = 45.0f;
                float sizeH = 60.0f;
                Rectangle fireDst = { orb->position.x, orb->position.y, sizeW, sizeH };
                Vector2 origin = { sizeW / 2.0f, sizeH / 2.0f }; // Center pivot for rotation
                
                float angle = atan2f(orb->velocity.y, orb->velocity.x) * (180.0f / 3.14159265f);
                float rotation = angle + 90.0f; // Align UP-pointing flame to velocity
                
                DrawTexturePro(orbFireTex, fireSrc, fireDst, origin, rotation, (Color){0, 255, 255, 255});
            } else {
                // Fallback
                float pulseScale = 1.0f + sinf(orb->pulseTimer * 10.0f) * 0.15f;
                DrawCircleV(orb->position, orb->radius * pulseScale + 4, (Color){0, 255, 255, 60});
                DrawCircleV(orb->position, orb->radius * pulseScale, (Color){0, 255, 255, 255});
                DrawCircleV(orb->position, orb->radius * pulseScale * 0.6f, (Color){255, 255, 255, 180});
                for (int j = 1; j <= 3; j++) {
                    float alpha = 1.0f - (float)j * 0.3f;
                    DrawCircleV(orb->position, orb->radius * (1.0f - (float)j * 0.2f), 
                        (Color){0, 255, 255, (unsigned char)(alpha * 120)});
                }
            }
        }
    }
}
