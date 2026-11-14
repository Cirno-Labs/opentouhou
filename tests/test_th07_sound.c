#include "th07/game.h"
#include "th07/sound.h"

#include "core/io/writer.h"

#include "test.h"

#include <string.h>

#define RATE 48000

typedef struct {
    const char *name;
    uint32_t offset;
    uint32_t size;
    int32_t loop_start;
    int32_t loop_end;
    uint16_t channels;
    uint16_t bits;
} TRACK;

static void M_WriteTrack(WRITER *const writer, const TRACK *const track)
{
    char name[16] = { 0 };
    if (track->name != nullptr) {
        const size_t length = strlen(track->name);
        memcpy(
            name, track->name, length < sizeof(name) ? length : sizeof(name));
    }
    Writer_WriteBytes(writer, name, sizeof(name));
    Writer_WriteU32(writer, track->offset);
    Writer_WriteU32(writer, track->size);
    Writer_WriteI32(writer, track->loop_start);
    Writer_WriteI32(writer, track->loop_end);
    const uint16_t channels = track->channels != 0 ? track->channels : 2;
    const uint16_t bits = track->bits != 0 ? track->bits : 16;
    Writer_WriteU16(writer, 1);
    Writer_WriteU16(writer, channels);
    Writer_WriteU32(writer, RATE);
    Writer_WriteU32(writer, RATE * channels * bits / 8);
    Writer_WriteU16(writer, (uint16_t)(channels * bits / 8));
    Writer_WriteU16(writer, bits);
    Writer_WriteU16(writer, 0);
    Writer_WriteU16(writer, 0);
}

// Returns thbgm.fmt with tracks and then an entry with an empty name,
// unless terminate is false.
static uint8_t *M_List(
    const TRACK *const tracks, const int32_t count, const bool terminate,
    size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    for (int32_t i = 0; i < count; i++) {
        M_WriteTrack(&writer, &tracks[i]);
    }
    if (terminate) {
        M_WriteTrack(&writer, &(TRACK) {});
    }
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    return data;
}

static RESULT M_ReadList(
    const TRACK *const tracks, const int32_t count, const bool terminate,
    BGM_LIST *const out)
{
    size_t size;
    uint8_t *const data = M_List(tracks, count, terminate, &size);
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    const RESULT result = Bgm_ReadList(&reader, &g_HeapAllocator, out);
    Mem_Free(data);
    return result;
}

static void M_CheckQueue(const int32_t *const expected)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        CHECK_EQ_INT(g_SoundEvents.queue[i], expected[i]);
    }
}

static const TRACK m_Tracks[] = {
    { "th07_00.wav", 16, 64, 0, 4 * 16, 0, 0 },
    { "th07_01.wav", 16 + 4 * 16, 4 * 32, 4 * 2, 4 * 5, 0, 0 },
    { "th07_02.wav", 16 + 4 * 32, 4 * 16, 0, 4 * 3, 0, 0 },
};

TEST(queue_holds_five_sounds_in_order)
{
    Sound_Reset();
    const int32_t sounds[] = { SOUND_GRAZE,  SOUND_ITEM,  SOUND_DAMAGE,
                               SOUND_BORDER, SOUND_PAUSE, SOUND_EXTEND };
    for (size_t i = 0; i < ARRAY_SIZE(sounds); i++) {
        Sound_Queue(sounds[i]);
    }
    M_CheckQueue(sounds);
}

TEST(queue_skips_a_sound_already_queued_and_takes_indices_up_to_0x7f)
{
    Sound_Reset();
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(-1);
    Sound_Queue(0x80);
    Sound_Queue(0x7f);
    Sound_Queue(SOUND_COUNT);
    M_CheckQueue((const int32_t[]) { SOUND_GRAZE, 0x7f, SOUND_COUNT, -1, -1 });
}

TEST(play_names_the_wav_track_and_ignores_a_path_without_a_dot)
{
    Sound_Reset();
    Sound_PlayMusic("bgm/th07_13b.mid");
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th07_13b.wav");
    Sound_PlayMusic("bgm/x.m");
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wav");
    Sound_PlayMusic("bgm/x.midi");
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wavi");
    Sound_PlayMusic("bgm/nodot");
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wavi");
    char path[SOUND_PATH_SIZE];
    memset(path, 'a', sizeof(path) - 5);
    memcpy(path + sizeof(path) - 5, ".mid", 5);
    Sound_PlayMusic(path);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wavi");
}

TEST(play_and_stop_drop_an_earlier_fade_and_pause)
{
    Sound_Reset();
    Sound_FadeOutMusic(3.0f);
    Sound_PauseMusic();
    Sound_PlayMusic("bgm/th07_01.mid");
    CHECK(!g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_KEEP);
    Sound_FadeOutMusic(3.0f);
    Sound_ResumeMusic();
    Sound_StopMusic();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_STOP);
    CHECK(!g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_KEEP);
    Sound_FadeOutMusic(3.0f);
    CHECK(!g_SoundEvents.fade_out);
}

TEST(the_later_of_pause_and_resume_wins)
{
    Sound_Reset();
    Sound_PauseMusic();
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_MUSIC);
    Sound_ResumeMusic();
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_RESUME_MUSIC);
    Sound_PauseMusic();
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_MUSIC);
}

TEST(fade_seconds_stretch_while_slow_and_cut_to_whole_seconds)
{
    const float speeds[] = { 1.0f, 0.5f, 0.3f, 2.0f, 0.0f };
    const int32_t expected[] = { 3, 7, 11, 3, 3 };
    for (size_t i = 0; i < ARRAY_SIZE(speeds); i++) {
        Sound_Reset();
        g_GameSpeed = speeds[i];
        Sound_FadeOutMusic(3.5f);
        CHECK(g_SoundEvents.fade_out);
        CHECK_EQ_INT(g_SoundEvents.fade_seconds, expected[i]);
    }
    g_GameSpeed = 1.0f;
}

TEST(list_reads_the_tracks_up_to_the_empty_name)
{
    BGM_LIST list;
    CHECK(Result_Log(M_ReadList(m_Tracks, 3, true, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 3);
    const BGM_TRACK *const track = &list.tracks[1];
    CHECK_EQ_STR(track->name, "th07_01.wav");
    CHECK_EQ_INT(track->offset, 80);
    CHECK_EQ_INT(track->size, 128);
    CHECK_EQ_INT(track->loop_start, 8);
    CHECK_EQ_INT(track->loop_end, 20);
    CHECK_EQ_INT(track->format_tag, 1);
    CHECK_EQ_INT(track->channels, 2);
    CHECK_EQ_INT(track->sample_rate, RATE);
    CHECK_EQ_INT(track->block_align, 4);
    CHECK_EQ_INT(track->bits_per_sample, 16);
    Bgm_FreeList(&list);
    CHECK(list.tracks == nullptr);
}

TEST(list_ends_at_an_end_entry_of_one_byte)
{
    size_t size;
    uint8_t *const tracks = M_List(m_Tracks, 3, false, &size);
    uint8_t *const data = Mem_Alloc(size + 1);
    memcpy(data, tracks, size);
    data[size] = 0;
    READER reader;
    Reader_OpenBuffer(&reader, data, size + 1, nullptr);
    BGM_LIST list;
    CHECK(Result_Log(
        Bgm_ReadList(&reader, &g_HeapAllocator, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 3);
    Bgm_FreeList(&list);
    Mem_Free(data);
    Mem_Free(tracks);
}

TEST(list_without_an_end_entry_or_a_track_fails)
{
    BGM_LIST list;
    CHECK(!Result_Ignore(M_ReadList(m_Tracks, 3, false, &list)));
    CHECK(!Result_Ignore(M_ReadList(m_Tracks, 0, true, &list)));
    const TRACK long_name = { "th07_01.wav_long", 0, 4, 0, 4, 0, 0 };
    CHECK(!Result_Ignore(M_ReadList(&long_name, 1, true, &list)));
    CHECK(list.tracks == nullptr);
}

TEST(find_track_takes_the_file_name_and_falls_back_to_the_first)
{
    BGM_LIST list;
    CHECK(Result_Log(M_ReadList(m_Tracks, 3, true, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Bgm_FindTrack(&list, "bgm/th07_02.wav"), 2);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "a\\b\\th07_01.wav"), 1);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "th07_01.wav"), 1);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "bgm/th07_09.wav"), 0);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "bgm/th07_02.mid"), 0);
    Bgm_FreeList(&list);
}
