#ifndef AUDIO_H
#define AUDIO_H

#include "raylib.h"
#include "boss.h"

// SFX IDs for playing sounds
typedef enum {
    SFX_LAUGH,
    SFX_DAMAGE,
    SFX_ALARM,
    SFX_HITS,
    SFX_SLASH,
    SFX_COUNT
} SfxID;

// BGM Track IDs
typedef enum {
    BGM_NONE = -1,
    BGM_PHRASE12 = 0,
    BGM_PHRASE3 = 1,
    BGM_PHRASE4 = 2,
    BGM_ENDING = 3
} BgTrack;

// Global Audio control functions
void Audio_InitDevice(void);
void Audio_CloseDevice(void);

// Load/Unload boss assets
void Audio_LoadBossAssets(void);
void Audio_UnloadBossAssets(void);

// SFX controls
void Audio_PlaySFX(SfxID id);
void Audio_StopSFX(SfxID id);

// Music controls & updates
void Audio_Update(float dt, Boss *boss);
void Audio_StartIntroMusic(void);
void Audio_PlaySFX_OnRoar(void);
void Audio_ResetBossFight(void);

// Subtitle controls
void Audio_DrawSubtitles(void);

#endif // AUDIO_H
