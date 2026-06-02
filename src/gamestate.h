#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "raylib.h"

extern Font gGameFont;

#ifndef DrawText
#define DrawText(text, x, y, size, ...) DrawTextEx(gGameFont, text, (Vector2){(float)(x), (float)(y)}, (float)(size), 1.0f, __VA_ARGS__)
#endif

#ifndef MeasureText
#define MeasureText(text, size) ((int)MeasureTextEx(gGameFont, text, (float)(size), 1.0f).x)
#endif

typedef enum {
    STATE_PLAYING,
    STATE_WIN,
    STATE_LOSE
} GameState;

void DrawUI(int playerHP, int bossHP, int bossMaxHP);
void DrawWinScreen(void);
void DrawLoseScreen(void);

#endif // GAMESTATE_H