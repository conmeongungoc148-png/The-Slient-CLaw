#include <stdio.h>
#include <string.h>

typedef struct {
    float startTime;
    float endTime;
    char text[256];
} SubtitleEntry;

#define MAX_SUBTITLES 128
static SubtitleEntry subtitles[MAX_SUBTITLES];
static int subtitleCount = 0;

static void LoadSRT(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open %s\n", filename);
        return;
    }
    
    subtitleCount = 0;
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
                
                if (subtitleCount < MAX_SUBTITLES) {
                    subtitles[subtitleCount++] = currentEntry;
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
            int parsed = sscanf(line, "%d:%d:%d,%d --> %d:%d:%d,%d", 
                       &sh, &sm, &ss, &sms, &eh, &em, &es, &ems);
            if (parsed == 8) {
                float start = sh * 3600.0f + sm * 60.0f + ss + sms / 1000.0f;
                float end = eh * 3600.0f + em * 60.0f + es + ems / 1000.0f;
                
                currentEntry.startTime = start - 3600.0f;
                currentEntry.endTime = end - 3600.0f;
                
                state = 2;
            } else {
                printf("Failed to parse timing line (parsed %d/8): '%s'\n", parsed, line);
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
        
        if (subtitleCount < MAX_SUBTITLES) {
            subtitles[subtitleCount++] = currentEntry;
        }
    }
    
    fclose(file);
}

int main(void) {
    LoadSRT("boss/assets/audio/music/boss_subtitle 1.srt");
    printf("Total subtitles loaded: %d\n", subtitleCount);
    for (int i = 0; i < subtitleCount; i++) {
        printf("[%d] %.3f -> %.3f: '%s'\n", i, subtitles[i].startTime, subtitles[i].endTime, subtitles[i].text);
    }
    return 0;
}
