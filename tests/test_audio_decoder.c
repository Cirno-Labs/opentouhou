// SPDX-License-Identifier: GPL-3.0-only

#include "core/audio/decoder.h"
#include "core/io/writer.h"

#include "test.h"

static READER M_Wave(const uint16_t channels, const uint32_t frames)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Writer_WriteBytes(&writer, "RIFF", 4);
    Writer_WriteU32(&writer, 36 + frames * channels * 2);
    Writer_WriteBytes(&writer, "WAVEfmt ", 8);
    Writer_WriteU32(&writer, 16);
    Writer_WriteU16(&writer, 1);
    Writer_WriteU16(&writer, channels);
    Writer_WriteU32(&writer, 22050);
    Writer_WriteU32(&writer, 22050 * channels * 2);
    Writer_WriteU16(&writer, channels * 2);
    Writer_WriteU16(&writer, 16);
    Writer_WriteBytes(&writer, "data", 4);
    Writer_WriteU32(&writer, frames * channels * 2);
    for (uint32_t frame = 0; frame < frames; frame++) {
        for (uint16_t channel = 0; channel < channels; channel++) {
            Writer_WriteI16(&writer, (int16_t)(frame * 13 + channel * 100));
        }
    }
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &g_HeapAllocator);
    return reader;
}

static void M_CheckSamples(
    AUDIO_DECODER *const decoder, uint32_t position, const uint32_t end,
    const uint32_t channels)
{
    while (position < end) {
        const float *data;
        uint32_t frames;
        CHECK(Result_Log(
            AudioDecoder_Read(decoder, &data, &frames), LOG_LEVEL_ERROR));
        if (frames == 0) {
            TEST_FAIL("audio ended at frame %u before %u", position, end);
            return;
        }
        CHECK(frames <= end - position);
        for (uint32_t frame = 0; frame < frames; frame++) {
            const float left = (float)((position + frame) * 13) / 32768.0f;
            const float right =
                left + (channels == 2 ? 100.0f / 32768.0f : 0.0f);
            CHECK(data[frame * 2] == left);
            CHECK(data[frame * 2 + 1] == right);
        }
        position += frames;
    }
    const float *data;
    uint32_t frames;
    CHECK(Result_Log(
        AudioDecoder_Read(decoder, &data, &frames), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(frames, 0);
}

TEST(mono_duplicates_at_source_rate)
{
    READER reader = M_Wave(1, 777);
    AUDIO_DECODER *decoder = nullptr;
    CHECK(Result_Log(AudioDecoder_Open(&reader, &decoder), LOG_LEVEL_ERROR));
    if (decoder == nullptr) {
        Reader_Close(&reader);
        return;
    }
    const AUDIO_INFO info = AudioDecoder_GetInfo(decoder);
    CHECK_EQ_INT(info.sample_rate, 22050);
    CHECK_EQ_INT(info.channels, 1);
    CHECK(info.length_known);
    CHECK_EQ_INT(info.length, 777);
    M_CheckSamples(decoder, 0, 777, 1);
    CHECK(Result_Log(AudioDecoder_Seek(decoder, 555), LOG_LEVEL_ERROR));
    M_CheckSamples(decoder, 555, 777, 1);
    AudioDecoder_Free(decoder);
    Reader_Close(&reader);
}

TEST(stereo_and_repeated_seeks_preserve_samples)
{
    READER reader = M_Wave(2, 1234);
    AUDIO_DECODER *decoder = nullptr;
    CHECK(Result_Log(AudioDecoder_Open(&reader, &decoder), LOG_LEVEL_ERROR));
    if (decoder == nullptr) {
        Reader_Close(&reader);
        return;
    }
    M_CheckSamples(decoder, 0, 1234, 2);
    const uint64_t positions[] = { 555, 1, 0, 1233, 1234, 17 };
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        CHECK(Result_Log(
            AudioDecoder_Seek(decoder, positions[i]), LOG_LEVEL_ERROR));
        M_CheckSamples(decoder, (uint32_t)positions[i], 1234, 2);
    }
    CHECK(!Result_Ignore(AudioDecoder_Seek(decoder, 1235)));
    AudioDecoder_Free(decoder);
    Reader_Close(&reader);
}

TEST(rejects_unknown_audio)
{
    READER reader;
    Reader_OpenBuffer(&reader, "not audio", 9);
    AUDIO_DECODER *decoder = nullptr;
    CHECK(!Result_Ignore(AudioDecoder_Open(&reader, &decoder)));
    CHECK(decoder == nullptr);
    AudioDecoder_Free(decoder);
}
