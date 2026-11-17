#include "th01/gameplay_internal.h"
#include "th01_assets.h"

#include "core/base/arena.h"

#include <stdlib.h>
#include <string.h>

static FILE *m_Log;

static void M_VerifyDiagnostic(const char *message, const char *file, int line)
{
    char text[4096];
    rewind(m_Log);
    const size_t size = fread(text, 1, sizeof(text) - 1, m_Log);
    text[size] = '\0';
    if (strstr(text, "TH01 audio: Invalid MDRV song index") == nullptr) {
        _Exit(23);
    }
    fclose(m_Log);
    Log_SetFile(nullptr);
}

int main(void)
{
    ARENA arena;
    Arena_Init(&arena, "audio diagnostic", 8 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&arena);
    GAMEPLAY_ASSETS *const assets = Th01TestAssets_Create(allocator);
    VIDEO video;
    Video_Init(&video, allocator);
    SESSION session = { .settings = { .music = SETTINGS_MUSIC_FM } };
    GAMEPLAY *game;
    if (!Result_Log(
            Gameplay_Prepare(allocator, assets, &video, &session, &game),
            LOG_LEVEL_ERROR)) {
        return 24;
    }
    m_Log = tmpfile();
    if (m_Log == nullptr) {
        return 25;
    }
    Log_SetFile(m_Log);
    Log_SetFatalHandler(M_VerifyDiagnostic);
    Gameplay_Play(game, GAMEPLAY_SONG_COUNT);
    return 26;
}
