#include "th02/audio.h"

#include "core/audio/live.h"

#define M_BLOCK_FRAMES 800
#define M_QUEUE_FRAMES (M_BLOCK_FRAMES * 4)

struct GAME_AUDIO {
    AUDIO_LIVE *input;
    OPNA_FRAME raw[M_BLOCK_FRAMES];
    float samples[M_BLOCK_FRAMES * 2];
    float previous_input[2], previous_output[2];
};

RESULT GameAudio_Prepare(
    const ALLOCATOR *const allocator, GAME_AUDIO **const out)
{
    *out = nullptr;
    const AUDIO_GROUP group = AUDIO_GROUP_MUSIC;
    AUDIO_LIVE *input = nullptr;
    MUST(AudioLive_Create(&group, 1, M_QUEUE_FRAMES, &input));
    GAME_AUDIO *const output = Allocator_AllocZ(allocator, sizeof(*output));
    output->input = input;
    *out = output;
    return OK;
}

RESULT GameAudio_Render(GAME_AUDIO *const output, PMD *const pmd, size_t frames)
{
    if (output == nullptr) {
        Pmd_Render(pmd, nullptr, frames);
        return OK;
    }
    while (frames != 0) {
        const size_t count = frames < M_BLOCK_FRAMES ? frames : M_BLOCK_FRAMES;
        Pmd_Render(pmd, output->raw, count);
        for (size_t i = 0; i < count; i++) {
            const OPNA_FRAME *const raw = &output->raw[i];
            const int32_t ssg = raw->ssg[0] + raw->ssg[1] + raw->ssg[2];
            for (size_t channel = 0; channel < 2; channel++) {
                const float input =
                    (float)(raw->fm_rhythm[channel] + ssg) / 65536.0f;
                const float filtered = input - output->previous_input[channel]
                    + 0.99738543f * output->previous_output[channel];
                output->previous_input[channel] = input;
                output->previous_output[channel] = filtered;
                output->samples[i * 2 + channel] = filtered;
            }
        }
        const float *const samples[] = { output->samples };
        MUST(AudioLive_Submit(output->input, samples, (uint32_t)count));
        frames -= count;
    }
    return OK;
}

void GameAudio_Free(GAME_AUDIO *const output)
{
    if (output != nullptr) {
        AudioLive_Free(output->input);
        output->input = nullptr;
    }
}
