#include "audio.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
// Manually declare ShellExecuteA to avoid including windows.h, preventing collisions with Raylib
#ifndef HWND
typedef void* HWND;
#endif
#ifndef HINSTANCE
typedef void* HINSTANCE;
#endif

#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) HINSTANCE __stdcall ShellExecuteA(
    HWND hwnd,
    const char *lpOperation,
    const char *lpFile,
    const char *lpParameters,
    const char *lpDirectory,
    int nShowCmd
);
#ifdef __cplusplus
}
#endif

#define SW_SHOWNORMAL 1
#endif

typedef struct {
    float startTime;
    float endTime;
    char text[256];
} SubtitleEntry;

#define MAX_SUBTITLES 128
static SubtitleEntry sub1[MAX_SUBTITLES];
static int sub1Count = 0;
static SubtitleEntry sub2[MAX_SUBTITLES];
static int sub2Count = 0;

static SubtitleEntry *activeSubs = NULL;
static int activeSubCount = 0;

static Music fight1Music;
static bool fight1MusicLoaded = false;
static Music fight2Music;
static bool fight2MusicLoaded = false;
static bool playEndingMusic = false;

static Sound damageSfx;
static Sound alarmSfx;
static Sound hitsSfx;
static Sound slashSfx;

static bool introMusicStarted = false;
static bool videoLaunched = false;

// Playback timers starting exactly when cutscene starts
static float bgmTimer = 0.0f;
static bool bgmTimerActive = false;
static float endingMusicTimer = 0.0f;

static void CopySubtitleFile(void) {
    FILE *src = fopen("boss/assets/audio/music/boss_subtitle 2.srt", "rb");
    if (!src) {
        TraceLog(LOG_WARNING, "SRT COPY: Failed to open source boss_subtitle 2.srt");
        return;
    }
    FILE *dst = fopen("boss/assets/audio/music/fight2.srt", "wb");
    if (!dst) {
        TraceLog(LOG_WARNING, "SRT COPY: Failed to open destination fight2.srt");
        fclose(src);
        return;
    }
    char buf[4096];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, bytes, dst);
    }
    fclose(src);
    fclose(dst);
    TraceLog(LOG_INFO, "SRT COPY: Successfully copied boss_subtitle 2.srt to fight2.srt");
}

static void LaunchVideo(void) {
#ifdef _WIN32
    ShellExecuteA(NULL, "open", "boss\\assets\\audio\\music\\fight2.mov", NULL, NULL, SW_SHOWNORMAL);
    TraceLog(LOG_INFO, "VIDEO: ShellExecute called for fight2.mov");
#else
    system("xdg-open boss/assets/audio/music/fight2.mov &");
#endif
}

static void LoadSRT(const char *filename, SubtitleEntry *destList, int *destCount, float offset) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        TraceLog(LOG_WARNING, "Failed to open subtitle file: %s", filename);
        *destCount = 0;
        return;
    }
    
    int count = 0;
    char line[512];
    int state = 0; // 0: expecting index, 1: expecting timing, 2: expecting text
    SubtitleEntry currentEntry;
    currentEntry.text[0] = '\0';
    
    while (fgets(line, sizeof(line), file)) {
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[len - 1] = '\0';
            len--;
        }
        
        if (len == 0) {
            if (state == 2) {
                char cleanText[256];
                int j = 0;
                for (int i = 0; currentEntry.text[i] != '\0' && j < 255; i++) {
                    if (currentEntry.text[i] == '<') {
                        while (currentEntry.text[i] != '\0' && currentEntry.text[i] != '>') {
                            i++;
                        }
                    } else {
                        cleanText[j++] = currentEntry.text[i];
                    }
                }
                cleanText[j] = '\0';
                strcpy(currentEntry.text, cleanText);
                
                if (count < MAX_SUBTITLES) {
                    destList[count++] = currentEntry;
                }
                currentEntry.text[0] = '\0';
                state = 0;
            }
            continue;
        }
        
        if (state == 0) {
            state = 1;
        } else if (state == 1) {
            int sh, sm, ss, sms;
            int eh, em, es, ems;
            if (sscanf(line, "%d:%d:%d,%d --> %d:%d:%d,%d", 
                       &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) == 8) {
                float start = sh * 3600.0f + sm * 60.0f + ss + sms / 1000.0f;
                float end = eh * 3600.0f + em * 60.0f + es + ems / 1000.0f;
                
                // Subtract custom offset
                currentEntry.startTime = start - offset;
                currentEntry.endTime = end - offset;
                
                state = 2;
            }
        } else if (state == 2) {
            if (currentEntry.text[0] != '\0') {
                strcat(currentEntry.text, "\n");
            }
            strncat(currentEntry.text, line, sizeof(currentEntry.text) - strlen(currentEntry.text) - 1);
        }
    }
    
    if (state == 2 && currentEntry.text[0] != '\0') {
        char cleanText[256];
        int j = 0;
        for (int i = 0; currentEntry.text[i] != '\0' && j < 255; i++) {
            if (currentEntry.text[i] == '<') {
                while (currentEntry.text[i] != '\0' && currentEntry.text[i] != '>') {
                    i++;
                }
            } else {
                cleanText[j++] = currentEntry.text[i];
            }
        }
        cleanText[j] = '\0';
        strcpy(currentEntry.text, cleanText);
        
        if (count < MAX_SUBTITLES) {
            destList[count++] = currentEntry;
        }
    }
    
    fclose(file);
    *destCount = count;
    TraceLog(LOG_INFO, "SUBTITLES: Loaded %d subtitles from %s", count, filename);
}

void Audio_InitDevice(void) {
    InitAudioDevice();
}

void Audio_CloseDevice(void) {
    CloseAudioDevice();
}

void Audio_LoadBossAssets(void) {
    // Try loading fight1.ogg first, then fight1.mp3
    if (FileExists("boss/assets/audio/music/fight1.ogg")) {
        fight1Music = LoadMusicStream("boss/assets/audio/music/fight1.ogg");
        fight1MusicLoaded = IsMusicReady(fight1Music);
    } else if (FileExists("boss/assets/audio/music/fight1.mp3")) {
        fight1Music = LoadMusicStream("boss/assets/audio/music/fight1.mp3");
        fight1MusicLoaded = IsMusicReady(fight1Music);
    } else {
        fight1MusicLoaded = false;
    }
    
    if (fight1MusicLoaded) {
        fight1Music.looping = false;
        TraceLog(LOG_INFO, "BGM: Loaded fight1 music stream");
    } else {
        TraceLog(LOG_WARNING, "BGM: Failed to load fight1 music stream");
    }

    // Try loading fight2.ogg internally if fight2.mov is not present
    fight2MusicLoaded = false;
    if (!FileExists("boss/assets/audio/music/fight2.mov")) {
        if (FileExists("boss/assets/audio/music/fight2.ogg")) {
            fight2Music = LoadMusicStream("boss/assets/audio/music/fight2.ogg");
            fight2MusicLoaded = IsMusicReady(fight2Music);
        }
    }
    
    if (fight2MusicLoaded) {
        fight2Music.looping = false;
        TraceLog(LOG_INFO, "BGM: Loaded fight2 music stream internally");
    }

    damageSfx = LoadSound("boss/assets/audio/sfx/damage.ogg");
    alarmSfx = LoadSound("boss/assets/audio/sfx/alarm.ogg");
    hitsSfx = LoadSound("boss/assets/audio/sfx/hits.ogg");
    slashSfx = LoadSound("boss/assets/audio/sfx/slash.ogg");
    
    // Load subtitle files
    // Subtract 3603.716f to shift 01:00:04,726 to exactly 1.01 seconds
    LoadSRT("boss/assets/audio/music/boss_subtitle 1.srt", sub1, &sub1Count, 3603.716f);
    LoadSRT("boss/assets/audio/music/boss_subtitle 2.srt", sub2, &sub2Count, 3600.0f);
    
    activeSubs = sub1;
    activeSubCount = sub1Count;
    
    introMusicStarted = false;
    videoLaunched = false;
    playEndingMusic = false;
    bgmTimer = 0.0f;
    bgmTimerActive = false;
    endingMusicTimer = 0.0f;
}

void Audio_UnloadBossAssets(void) {
    if (fight1MusicLoaded) {
        UnloadMusicStream(fight1Music);
        fight1MusicLoaded = false;
    }
    if (fight2MusicLoaded) {
        UnloadMusicStream(fight2Music);
        fight2MusicLoaded = false;
    }
    UnloadSound(damageSfx);
    UnloadSound(alarmSfx);
    UnloadSound(hitsSfx);
    UnloadSound(slashSfx);
    sub1Count = 0;
    sub2Count = 0;
    activeSubs = NULL;
    activeSubCount = 0;
    playEndingMusic = false;
    bgmTimer = 0.0f;
    bgmTimerActive = false;
    endingMusicTimer = 0.0f;
}

void Audio_PlaySFX(SfxID id) {
    switch (id) {
        case SFX_DAMAGE: PlaySound(damageSfx); break;
        case SFX_ALARM:  PlaySound(alarmSfx); break;
        case SFX_HITS:   PlaySound(hitsSfx); break;
        case SFX_SLASH:  PlaySound(slashSfx); break;
        default: break; // SFX_LAUGH is a safe no-op
    }
}

void Audio_StopSFX(SfxID id) {
    switch (id) {
        case SFX_DAMAGE: StopSound(damageSfx); break;
        case SFX_ALARM:  StopSound(alarmSfx); break;
        case SFX_HITS:   StopSound(hitsSfx); break;
        case SFX_SLASH:  StopSound(slashSfx); break;
        default: break; // SFX_LAUGH is a safe no-op
    }
}

void Audio_Update(float dt, Boss *boss) {
    if (fight1MusicLoaded && !playEndingMusic) {
        UpdateMusicStream(fight1Music);
    }
    if (fight2MusicLoaded && playEndingMusic) {
        UpdateMusicStream(fight2Music);
    }
    
    if (boss->state == BOSS_INTRO && !introMusicStarted) {
        if (fight1MusicLoaded) {
            PlayMusicStream(fight1Music);
            TraceLog(LOG_INFO, "BGM: Started playing fight1 BGM");
        }
        introMusicStarted = true;
        bgmTimer = 0.0f;
        bgmTimerActive = true;
    }
    
    if (bgmTimerActive && !playEndingMusic) {
        bgmTimer += dt;
    }
    
    if (playEndingMusic) {
        endingMusicTimer += dt;
    }
    
    if (boss->state == BOSS_DYING) {
        float timePlayed = bgmTimer;
        float totalLength = fight1MusicLoaded ? GetMusicTimeLength(fight1Music) : 0.0f;
        if (totalLength == 0.0f) totalLength = 430.0f; // fallback duration
        
        // Case 2: Early victory (if music is still playing and not yet finished)
        if (fight1MusicLoaded && timePlayed < totalLength - 1.0f) {
            StopMusicStream(fight1Music);
            bgmTimerActive = false;
            
            if (FileExists("boss/assets/audio/music/fight2.mov")) {
                if (!videoLaunched) {
                    CopySubtitleFile();
                    LaunchVideo();
                    videoLaunched = true;
                }
            } else if (fight2MusicLoaded && !playEndingMusic) {
                // Switch to playing fight2.ogg internally
                activeSubs = sub2;
                activeSubCount = sub2Count;
                PlayMusicStream(fight2Music);
                playEndingMusic = true;
                endingMusicTimer = 0.0f;
                TraceLog(LOG_INFO, "BGM: Playing fight2 ending music internally");
            }
            
            boss->defeated = true;
            boss->state = BOSS_DEFEATED;
        } else {
            // Case 1: Music finished naturally
            boss->defeated = true;
            boss->state = BOSS_DEFEATED;
        }
    }
}

void Audio_ResetBossFight(void) {
    if (fight1MusicLoaded) {
        StopMusicStream(fight1Music);
    }
    if (fight2MusicLoaded) {
        StopMusicStream(fight2Music);
    }
    activeSubs = sub1;
    activeSubCount = sub1Count;
    introMusicStarted = false;
    videoLaunched = false;
    playEndingMusic = false;
    bgmTimer = 0.0f;
    bgmTimerActive = false;
    endingMusicTimer = 0.0f;
}

void Audio_DrawSubtitles(void) {
    if (!introMusicStarted) return;
    
    float timePlayed = 0.0f;
    if (playEndingMusic) {
        timePlayed = endingMusicTimer;
    } else {
        timePlayed = bgmTimer;
    }
    
    const char *subText = NULL;
    for (int i = 0; i < activeSubCount; i++) {
        if (activeSubs && timePlayed >= activeSubs[i].startTime && timePlayed <= activeSubs[i].endTime) {
            subText = activeSubs[i].text;
            break;
        }
    }
    
    if (subText == NULL || strlen(subText) == 0) return;
    
    int fontSize = 24;
    float spacing = 1.0f;
    Vector2 textSize = MeasureTextEx(gGameFont, subText, (float)fontSize, spacing);
    
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    
    int posX = (screenWidth - (int)textSize.x) / 2;
    int posY = screenHeight - (int)textSize.y - 100;
    
    int paddingX = 15;
    int paddingY = 8;
    Rectangle bgRec = {
        (float)(posX - paddingX),
        (float)(posY - paddingY),
        textSize.x + paddingX * 2,
        textSize.y + paddingY * 2
    };
    
    DrawRectangleRec(bgRec, (Color){ 0, 0, 0, 180 });
    DrawRectangleLinesEx(bgRec, 1.5f, (Color){ 255, 215, 0, 220 });
    
    DrawTextEx(gGameFont, subText, (Vector2){ (float)posX + 2.0f, (float)posY + 2.0f }, (float)fontSize, spacing, BLACK);
    DrawTextEx(gGameFont, subText, (Vector2){ (float)posX, (float)posY }, (float)fontSize, spacing, RAYWHITE);
}
