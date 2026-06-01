#ifndef GAMESTATE_H
#define GAMESTATE_H

#include "raylib.h"

typedef enum {
    STATE_PLAYING,
    STATE_WIN,
    STATE_LOSE
} GameState;

void DrawUI(int playerHP, int bossHP, int bossMaxHP);
void DrawWinScreen(void);
void DrawLoseScreen(void);

#endif // GAMESTATE_H