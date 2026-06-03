#include "raylib.h"
#include <stdio.h>

int main(void) {
    InitWindow(200, 200, "Test");
    InitAudioDevice();
    
    Music m1 = LoadMusicStream("boss/assets/audio/music/fight1.ogg");
    printf("fight1.ogg loaded: %s, duration: %.2f\n", IsMusicReady(m1) ? "YES" : "NO", GetMusicTimeLength(m1));
    
    Music m2 = LoadMusicStream("boss/assets/audio/music/fight2.ogg");
    printf("fight2.ogg loaded: %s, duration: %.2f\n", IsMusicReady(m2) ? "YES" : "NO", GetMusicTimeLength(m2));
    
    UnloadMusicStream(m1);
    UnloadMusicStream(m2);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
