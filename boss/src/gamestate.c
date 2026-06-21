#include "gamestate.h"
#include <stdio.h>
#include <math.h>

static void DrawMinecraftHeart(int x, int y, int pixelSize, bool filled) {
    static const int filledMatrix[9][9] = {
        {0, 0, 1, 1, 0, 1, 1, 0, 0},
        {0, 1, 2, 2, 1, 2, 2, 1, 0},
        {1, 2, 4, 2, 2, 2, 2, 3, 1},
        {1, 2, 2, 2, 2, 2, 2, 3, 1},
        {0, 1, 2, 2, 2, 2, 3, 1, 0},
        {0, 0, 1, 2, 2, 3, 1, 0, 0},
        {0, 0, 0, 1, 3, 1, 0, 0, 0},
        {0, 0, 0, 0, 1, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0}
    };

    static const int emptyMatrix[9][9] = {
        {0, 0, 1, 1, 0, 1, 1, 0, 0},
        {0, 1, 5, 5, 1, 5, 5, 1, 0},
        {1, 5, 7, 5, 5, 5, 5, 6, 1},
        {1, 5, 5, 5, 5, 5, 5, 6, 1},
        {0, 1, 5, 5, 5, 5, 6, 1, 0},
        {0, 0, 1, 5, 5, 6, 1, 0, 0},
        {0, 0, 0, 1, 6, 1, 0, 0, 0},
        {0, 0, 0, 0, 1, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0}
    };

    Color black = (Color){0, 0, 0, 255};
    Color red = (Color){255, 19, 19, 255};
    Color darkRed = (Color){187, 19, 19, 255};
    Color glint = (Color){255, 201, 201, 255};

    Color darkGrey = (Color){25, 25, 25, 255};       // #191919
    Color shadowGrey = (Color){15, 15, 15, 255};     // #0f0f0f
    Color lightGrey = (Color){60, 60, 60, 255};      // #3c3c3c

    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int pixelType = filled ? filledMatrix[r][c] : emptyMatrix[r][c];
            if (pixelType == 0) continue;
            Color col;
            switch (pixelType) {
                case 1: col = black; break;
                case 2: col = red; break;
                case 3: col = darkRed; break;
                case 4: col = glint; break;
                case 5: col = darkGrey; break;
                case 6: col = shadowGrey; break;
                case 7: col = lightGrey; break;
                default: col = black; break;
            }
            DrawRectangle(x + c * pixelSize, y + r * pixelSize, pixelSize, pixelSize, col);
        }
    }
}

static void DrawLowHPVignette(int playerHP) {
    if (playerHP <= 0 || playerHP >= 4) return;

    // Calculate severity: hp=3 -> 0.33, hp=2 -> 0.66, hp=1 -> 1.0
    float severity = (4.0f - (float)playerHP) / 3.0f;
    if (severity > 1.0f) severity = 1.0f;
    if (severity < 0.0f) severity = 0.0f;

    // Pulse factor: speed increases with severity
    float pulseSpeed = 6.0f + severity * 9.0f;
    float pulse = (sinf(GetTime() * pulseSpeed) + 1.0f) * 0.5f;

    // Combine severity and pulsing: base alpha + pulse alpha
    float alphaFactor = (0.3f + 0.7f * pulse) * severity;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // Layer 1: Inner (thick and soft)
    int thick1 = 160;
    Color edgeCol1 = (Color){200, 0, 0, (unsigned char)(100 * alphaFactor)};
    Color centerCol1 = (Color){200, 0, 0, 0};

    // Layer 2: Outer (thin and intense)
    int thick2 = 50;
    Color edgeCol2 = (Color){255, 0, 0, (unsigned char)(180 * alphaFactor)};
    Color centerCol2 = (Color){255, 0, 0, 0};

    // Draw Layer 1 (Thicker, softer)
    DrawRectangleGradientV(0, 0, sw, thick1, edgeCol1, centerCol1);
    DrawRectangleGradientV(0, sh - thick1, sw, thick1, centerCol1, edgeCol1);
    DrawRectangleGradientH(0, 0, thick1, sh, edgeCol1, centerCol1);
    DrawRectangleGradientH(sw - thick1, 0, thick1, sh, centerCol1, edgeCol1);

    // Draw Layer 2 (Thinner, more intense)
    DrawRectangleGradientV(0, 0, sw, thick2, edgeCol2, centerCol2);
    DrawRectangleGradientV(0, sh - thick2, sw, thick2, centerCol2, edgeCol2);
    DrawRectangleGradientH(0, 0, thick2, sh, edgeCol2, centerCol2);
    DrawRectangleGradientH(sw - thick2, 0, thick2, sh, centerCol2, edgeCol2);
}

void DrawUI(int playerHP, int bossHP, int bossMaxHP, bool isOutro) {
    // === Low HP Vignette ===
    if (!isOutro) {
        DrawLowHPVignette(playerHP);
    }

    // === Player HP (góc trái dưới) ===
    int heartSpacing = 35;
    int startX = 30;
    int startY = 30;

    DrawText("PLAYER", startX, startY - 5, 16, RAYWHITE);
    for (int i = 0; i < 10; i++) {
        bool filled = (i < playerHP);
        DrawMinecraftHeart(startX + i * heartSpacing, startY + 15, 3, filled);
    }

    // === Boss HP Bar (giữa trên) ===
    int barWidth = 500;
    int barHeight = 25;
    int barX = (GetScreenWidth() - barWidth) / 2;
    int barY = 30;

    float hpPercent = (float)bossHP / bossMaxHP;

    // Background
    DrawRectangle(barX - 2, barY - 2, barWidth + 4, barHeight + 4, (Color){40, 40, 40, 255});
    // HP fill
    Color hpColor = GREEN;
    if (hpPercent < 0.3f) hpColor = RED;
    else if (hpPercent < 0.7f) hpColor = ORANGE;
    DrawRectangle(barX, barY, (int)(barWidth * hpPercent), barHeight, hpColor);
    // Border
    DrawRectangleLines(barX, barY, barWidth, barHeight, WHITE);
    // Text
    DrawText("AGIS", barX + barWidth / 2 - 20, barY + 3, 20, WHITE);
}

void DrawWinScreen(void) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 180});
    
    const char *text = "VICTORY!";
    int fontSize = 80;
    int textW = MeasureText(text, fontSize);
    DrawText(text, (GetScreenWidth() - textW) / 2, GetScreenHeight() / 2 - 60, fontSize, GOLD);

    const char *sub = "Press R to Retry  |  Press ESC to Quit";
    int subW = MeasureText(sub, 24);
    DrawText(sub, (GetScreenWidth() - subW) / 2, GetScreenHeight() / 2 + 40, 24, RAYWHITE);
}

void DrawLoseScreen(void) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){80, 0, 0, 180});
    
    const char *text = "DEFEATED";
    int fontSize = 80;
    int textW = MeasureText(text, fontSize);
    DrawText(text, (GetScreenWidth() - textW) / 2, GetScreenHeight() / 2 - 60, fontSize, RED);

    const char *sub = "Press R to Retry  |  Press ESC to Quit";
    int subW = MeasureText(sub, 24);
    DrawText(sub, (GetScreenWidth() - subW) / 2, GetScreenHeight() / 2 + 40, 24, RAYWHITE);
}