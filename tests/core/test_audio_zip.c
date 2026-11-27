#include "core/archive/zip.h"
#include "core/audio.h"
#include "core/io/writer.h"

#include "test.h"
#include "zip_builder.h"

#include <SDL3/SDL.h>

static READER M_OpenMusic(const bool deflate)
{
    const uint8_t pcm[] = { 0, 32, 0, 224, 0, 16, 0, 240 };
    const ZIP_BUILDER_ENTRY entry = { "music", pcm, sizeof(pcm), 0, deflate };
    ZIP_BUILDER_ARCHIVE built = ZipBuilder_Build(&entry, 1);
    READER input;
    Reader_OpenOwnedBuffer(
        &input, built.data, built.size, &g_HeapAllocator, nullptr);
    ARCHIVE *archive;
    CHECK(Result_Log(
        Archive_Open(&g_ZipFormat, &input, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    VFS_SOURCE *const source = Archive_ToSource(archive, "");
    READER music;
    CHECK(Result_Log(
        source->funcs->open(source, "music", VFS_STREAM, &music),
        LOG_LEVEL_ERROR));
    source->funcs->close(source);
    return music;
}

TEST(zip_music_transfers_to_the_audio_decoder)
{
    CHECK(Result_Log(Audio_Init(), LOG_LEVEL_ERROR));
    for (int deflate = 0; deflate < 2; deflate++) {
        READER music = M_OpenMusic(deflate != 0);
        const AUDIO_PLAY_OPTIONS options = {
            .group = AUDIO_GROUP_MUSIC,
            .volume = 1.0f,
            .pitch = 1.0f,
        };
        const AUDIO_PCM_FORMAT format = { .sample_rate = 48000, .channels = 2 };
        AUDIO_STREAM *stream = nullptr;
        CHECK(Result_Log(
            AudioStream_LoadPcm(&music, format, &options, &stream),
            LOG_LEVEL_ERROR));
        CHECK_EQ_INT(Reader_Size(&music), 0);
        Reader_Close(&music);
        if (stream == nullptr) {
            continue;
        }
        const Uint64 deadline = SDL_GetTicks() + 5000;
        while (AudioStream_GetState(stream) == AUDIO_LOADING
               && SDL_GetTicks() < deadline) {
            SDL_Delay(1);
        }
        CHECK_EQ_INT(AudioStream_GetState(stream), AUDIO_READY);
        CHECK(Result_Log(AudioStream_GetResult(stream), LOG_LEVEL_ERROR));
        AUDIO_INFO info;
        CHECK(Result_Log(AudioStream_GetInfo(stream, &info), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(info.length, 2);
        CHECK_EQ_INT(info.channels, 2);
        AudioStream_Free(stream);
    }
    Audio_Shutdown();
}
