#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "raylib.h"
#include <stdbool.h>

#define MAX_PROJECTILES 20

typedef struct {
    Vector2 position;
    Vector2 velocity;
    Rectangle hitbox;
    bool active;
    int damage;
    float lifetime;     // Tự hủy sau X giây
    float radius;       // Bán kính để vẽ
} Projectile;

typedef struct {
    Projectile projectiles[MAX_PROJECTILES];
    int count;
} ProjectileManager;

void InitProjectileManager(ProjectileManager *pm);
void SpawnProjectile(ProjectileManager *pm, Vector2 pos, Vector2 vel, int damage);
void UpdateProjectiles(ProjectileManager *pm, float dt);
void DrawProjectiles(ProjectileManager *pm);
void UnloadProjectileAssets(void);
Texture2D GetOrbDamageTexture(void);

#endif // PROJECTILE_H