#include "projectile.h"
#include "boss_assets.h"
#include <math.h>

#define PROJECTILE_RADIUS 12.0f
#define PROJECTILE_LIFETIME 5.0f

void InitProjectileManager(ProjectileManager *pm) {
    pm->count = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        pm->projectiles[i].active = false;
    }
}

void SpawnProjectile(ProjectileManager *pm, Vector2 pos, Vector2 vel, int damage) {
    // Tìm slot trống
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->projectiles[i].active) {
            pm->projectiles[i].position = pos;
            pm->projectiles[i].velocity = vel;
            pm->projectiles[i].damage = damage;
            pm->projectiles[i].active = true;
            pm->projectiles[i].lifetime = PROJECTILE_LIFETIME;
            pm->projectiles[i].radius = PROJECTILE_RADIUS;
            pm->projectiles[i].hitbox = (Rectangle){
                pos.x - PROJECTILE_RADIUS,
                pos.y - PROJECTILE_RADIUS,
                PROJECTILE_RADIUS * 2,
                PROJECTILE_RADIUS * 2
            };
            pm->count++;
            return;
        }
    }
}

void UpdateProjectiles(ProjectileManager *pm, float dt) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->projectiles[i].active) continue;

        Projectile *p = &pm->projectiles[i];

        // Di chuyển
        p->position.x += p->velocity.x * dt;
        p->position.y += p->velocity.y * dt;

        // Cập nhật hitbox
        p->hitbox.x = p->position.x - p->radius;
        p->hitbox.y = p->position.y - p->radius;

        // Giảm lifetime
        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            p->active = false;
            pm->count--;
        }

        // Hủy nếu ra ngoài màn hình
        if (p->position.x < -100 || p->position.x > 2100 ||
            p->position.y < -100 || p->position.y > 1200) {
            p->active = false;
            pm->count--;
        }
    }
}

static Texture2D orbDamageTex = {0};
static bool orbDamageLoaded = false;

void DrawProjectiles(ProjectileManager *pm) {
    if (!orbDamageLoaded) {
        orbDamageTex = LoadTexture(GetBossAssetPath("assets/effects/vfx/orbdamage/sprite-sheet.png"));
        orbDamageLoaded = true;
    }

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!pm->projectiles[i].active) continue;

        Projectile *p = &pm->projectiles[i];

        if (orbDamageTex.id > 0) {
            float angle = atan2f(p->velocity.y, p->velocity.x) * (180.0f / 3.14159265f);
            int frame = (int)((PROJECTILE_LIFETIME - p->lifetime) * 12.0f) % 4;
            
            float scale = 0.18f; // Size adjustment to look nicely matched to original hitboxes
            float size = 128.0f * scale;
            Rectangle source = { (float)(frame * 128), 0.0f, 128.0f, 128.0f };
            Rectangle dest = { p->position.x, p->position.y, size, size };
            Vector2 origin = { size / 2.0f, size / 2.0f }; // Center origin for correct rotation pivot
            
            DrawTexturePro(orbDamageTex, source, dest, origin, angle, WHITE);
        } else {
            // Fallback
            DrawCircleV(p->position, p->radius + 4, (Color){200, 0, 50, 100});
            DrawCircleV(p->position, p->radius, (Color){255, 50, 100, 255});
            DrawCircleV(p->position, p->radius * 0.5f, (Color){255, 200, 200, 255});
        }
    }
}

void UnloadProjectileAssets(void) {
    if (orbDamageLoaded) {
        UnloadTexture(orbDamageTex);
        orbDamageLoaded = false;
        orbDamageTex = (Texture2D){0};
    }
}