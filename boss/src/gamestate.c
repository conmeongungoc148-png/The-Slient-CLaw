#include "gamestate.h"
#include <stdio.h>

void DrawUI(int playerHP, int bossHP, int bossMaxHP) {
    // === Player HP (góc trái dưới) ===
    int heartSize = 30;
    int heartSpacing = 35;
    int startX = 30;
    int startY = 30;

    DrawText("PLAYER", startX, startY - 5, 16, RAYWHITE);
    for (int i = 0; i < playerHP; i++) {
        DrawRectangle(startX + i * heartSpacing, startY + 15, heartSize - 5, heartSize - 5, RED);
        DrawRectangleLines(startX + i * heartSpacing, startY + 15, heartSize - 5, heartSize - 5, MAROON);
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