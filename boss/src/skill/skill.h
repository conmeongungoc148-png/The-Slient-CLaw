#ifndef SKILL_H
#define SKILL_H

#include "../boss.h"
#include "../collision.h"

// Hands offsets
#define LEFT_HAND_OFFSET_X (-100.0f)
#define LEFT_HAND_OFFSET_Y (50.0f)
#define RIGHT_HAND_OFFSET_X (100.0f)
#define RIGHT_HAND_OFFSET_Y (50.0f)

// General skill settings
#define PROJECTILE_SPEED_BASE 250.0f

#define LASER_CHARGE_TIME 2.0f
#define LASER_DURATION 2.0f
#define LASER_EXTEND_SPEED 300.0f

#define SLAM_WARNING_TIME 2.0f
#define SLAM_DURATION 1.0f

#define HAZARD_WARNING_TIME 1.2f

#define CLAW_WARNING_TIME 0.8f
#define CLAW_DURATION 0.6f

#define EXPLOSION_FRAMES 12
#define EXPLOSION_FW 96
#define EXPLOSION_FH 96

#define LASER_FRAMES 8
#define LASER_COLS 4
#define LASER_FW 300
#define LASER_FH 1309

// Projectile Skill
void DoProjectileAttack(Boss *boss, Vector2 playerPos, ProjectileManager *pm);
void DoBarrageAttack(Boss *boss, ProjectileManager *pm);

// Claw Skill
void StartClawAttack(Boss *boss, Vector2 playerPos);
void UpdateClawAttack(Boss *boss, float dt);
void DrawClawAttack(Boss *boss, Texture2D *splashTexs);

// Hazard Skill
void StartHazardAttack(Boss *boss);
void UpdateHazardAttack(Boss *boss, float dt);
void DrawHazardAttack(Boss *boss, Texture2D hazardTex);

// Laser Skill
void StartLaserAttack(Boss *boss, Vector2 playerPos);
void UpdateLaserAttack(Boss *boss, Vector2 playerPos, float dt);
void DrawLaserAttack(Boss *boss);

// Rain Skill
void StartRainAttack(Boss *boss);
void UpdateRainAttack(Boss *boss, ProjectileManager *pm, float dt);
void DrawRainAttack(Boss *boss);

// Slam Skill
void StartSlamAttack(Boss *boss, Vector2 playerPos);
void UpdateSlamAttack(Boss *boss, float dt);
void DrawSlamAttack(Boss *boss);

#endif // SKILL_H
