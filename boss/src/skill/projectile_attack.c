#include "../boss.h"
#include "../collision.h"
#include <stdlib.h>
#include <math.h>

#define PROJECTILE_SPEED_BASE 250.0f

void DoProjectileAttack(Boss *boss, Vector2 playerPos, ProjectileManager *pm) {
    // Spawn từ tay trái hoặc phải
    int hand = rand() % 2;
    Vector2 spawnPos = {
        boss->position.x + (hand == 0 ? LEFT_HAND_OFFSET_X : RIGHT_HAND_OFFSET_X) * boss->scale / 3.0f,
        boss->position.y + (hand == 0 ? LEFT_HAND_OFFSET_Y : RIGHT_HAND_OFFSET_Y) * boss->scale / 3.0f
    };

    Vector2 dir = NormalizeVec((Vector2){
        playerPos.x - spawnPos.x,
        playerPos.y - spawnPos.y
    });

    float speed = PROJECTILE_SPEED_BASE;

    switch (boss->phase) {
        case BOSS_PHASE_1:
            SpawnProjectile(pm, spawnPos, (Vector2){dir.x * speed, dir.y * speed}, 1);
            break;
        case BOSS_PHASE_2:
            speed *= 1.1f;
            SpawnProjectile(pm, spawnPos, (Vector2){dir.x * speed, dir.y * speed}, 1);
            {
                float angle = atan2f(dir.y, dir.x);
                float aL = angle - 0.3f, aR = angle + 0.3f;
                SpawnProjectile(pm, spawnPos, (Vector2){cosf(aL)*speed, sinf(aL)*speed}, 1);
                SpawnProjectile(pm, spawnPos, (Vector2){cosf(aR)*speed, sinf(aR)*speed}, 1);
            }
            break;
        case BOSS_PHASE_3:
            speed *= 1.15f;
            {
                float baseAngle = atan2f(dir.y, dir.x);
                for (int i = -1; i <= 1; i++) {
                    float a = baseAngle + i * 0.25f;
                    SpawnProjectile(pm, spawnPos, (Vector2){cosf(a)*speed, sinf(a)*speed}, 1);
                }
            }
            break;
        case BOSS_PHASE_4:
            // Phase 4: 5 viên fan (giảm từ 7) + speed thấp hơn (giảm từ 1.35)
            speed *= 1.25f;
            {
                float baseAngle = atan2f(dir.y, dir.x);
                for (int i = -2; i <= 2; i++) {
                    float a = baseAngle + i * 0.2f;
                    SpawnProjectile(pm, spawnPos, (Vector2){cosf(a)*speed, sinf(a)*speed}, 1);
                }
            }
            break;
    }
}

void DoBarrageAttack(Boss *boss, ProjectileManager *pm) {
    // Bắn 12 projectile theo 360° xung quanh boss (cả 2 tay)
    Vector2 leftHand = {
        boss->position.x + LEFT_HAND_OFFSET_X * boss->scale / 3.0f,
        boss->position.y + LEFT_HAND_OFFSET_Y * boss->scale / 3.0f
    };
    Vector2 rightHand = {
        boss->position.x + RIGHT_HAND_OFFSET_X * boss->scale / 3.0f,
        boss->position.y + RIGHT_HAND_OFFSET_Y * boss->scale / 3.0f
    };
    
    float speed = PROJECTILE_SPEED_BASE * 1.1f;
    float twoPi = 6.2831853f;
    
    // 6 viên từ tay trái (offset góc 0)
    for (int i = 0; i < 6; i++) {
        float angle = (twoPi / 12.0f) * i;
        SpawnProjectile(pm, leftHand,
            (Vector2){cosf(angle) * speed, sinf(angle) * speed}, 1);
    }
    // 6 viên từ tay phải (offset góc 30°)
    for (int i = 0; i < 6; i++) {
        float angle = (twoPi / 12.0f) * i + (twoPi / 24.0f);
        SpawnProjectile(pm, rightHand,
            (Vector2){cosf(angle) * speed, sinf(angle) * speed}, 1);
    }
    
    // Shake hiệu ứng nổ
    boss->shakeTimer = 0.3f;
    boss->shakeIntensity = 15.0f;
}
