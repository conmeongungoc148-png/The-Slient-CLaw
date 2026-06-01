Total Bytes: 46127
The following code has been modified to include a line number before every line, in the format: <line_number>: <original_line>. Please note that any changes targeting the original code should remove the line number, colon, and leading space.
    // Laser
    boss->laserActive = false;
    boss->laserChargeTime = 0.0f;
    boss->laserDuration = 0.0f;
    boss->laserStart = (Vector2){0, 0};
    boss->laserEnd = (Vector2){0, 0};
    boss->laserDirection = (Vector2){1, 0};

    // Slam
    boss->slamActive = false;
    boss->slamWarningTime = 0.0f;
    boss->slamTimer = 0.0f;
    boss->slamPos = (Vector2){0, 0};
    boss->shockwaveRadius = 0.0f;

    // Hazard
    boss->hazardCount = 0;
    for (int i = 0; i < 5; i++) {
        boss->hazardPositions[i] = (Vector2){0, 0};
        boss->hazardWarningTime[i] = 0.0f;
        boss->hazardActive[i] = false;
    }
    
    // Claw
    boss->clawActive = false;
    boss->clawZone = CLAW_ZONE_LEFT;
    boss->clawWarningTime = 0.0f;
    boss->clawDuration = 0.0f;

    // Animation
    boss->currentFrame = 0;
    boss->frameTimer = 0.0f;
    boss->animSpeed = 0.1f;

    // Visual
    boss->shakeTimer = 0.0f;
    boss->shakeIntensity = 0.0f;
    boss->scale = 3.0f;  // Boss to full screen
    
    // Death
    boss->deathTimer = 0.0f;
    boss->deathFlashTimer = 0.0f;

    // Atom bomb
    boss->fightTimer = 0.0f;
    boss->atomTimer = 0.0f;
    boss->atomTriggered = false;
    gAtomBombActive = 0;  // Reset flag khi init/restart

    // Hurtbox
    boss->hurtBox = (Rectangle){
<truncated 31496 bytes>
2000, 5000, 5000, 
                (Color){255, 255, 255, (unsigned char)(wAlpha * 200)});
        }
        
        return;
    }

    float shakeX = 0, shakeY = 0;
    if (boss->shakeTimer > 0) {
        shakeX = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
        shakeY = (float)(rand() % 20 - 10) * (boss->shakeTimer * boss->shakeIntensity);
    }

    int frameIdx = boss->currentFrame;
    float frameX = (float)frameIdx * BOSS_FRAME_W;

    Rectangle source = { frameX, 0, (float)BOSS_FRAME_W, (float)BOSS_FRAME_H };
    
    float destW = (float)BOSS_FRAME_W * boss->scale;
    float destH = (float)BOSS_FRAME_H * boss->scale;
    Rectangle dest = {
        boss->position.x + shakeX,
        boss->position.y + shakeY + cameraOffsetY,
        destW,
        destH
    };

    Vector2 origin = { destW / 2.0f, destH / 2.0f };

    // Tint theo phase
    Color tint = WHITE;
    if (boss->phase == BOSS_PHASE_2) tint = (Color){255, 200, 200, 255};
    if (boss->phase == BOSS_PHASE_3) tint = (Color){255, 100, 100, 255};
    if (boss->phase == BOSS_PHASE_4) tint = (Color){200, 50, 255, 255};  // Purple rage

    // Intro fade-in
    if (boss->state == BOSS_INTRO) {
        float alpha = boss->introTimer / INTRO_DURATION;
        if (alpha > 1.0f) alpha = 1.0f;
        tint.a = (unsigned char)(alpha * 255);
    }

    // Roar flash effect
    if (boss->state == BOSS_ROAR) {
        if ((int)(boss->roarTimer * 10) % 2 == 0) {
            tint = (Color){255, 255, 255, 255};
        } else {
            tint = (Color){255, 50, 50, 255};