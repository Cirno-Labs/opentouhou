#include "th02_music_room.c"

#include "th02/input.h"

#include "test.h"

typedef struct {
    TEST_MUSIC_VIEW view;
    MUSIC_ROOM_AUDIO_REQUEST audio[8];
    uint8_t old_playing[8], choices[16], colors[16];
    int begin_count, end_count, renders, flips, comments, audio_count, tracks;
    uint8_t drawing, visible;
} FIXTURE;

static void M_Begin(void *const context, const uint8_t selection)
{
    FIXTURE *const fixture = context;
    fixture->begin_count++;
    CHECK_EQ_INT(selection, m_Room.playing);
}

static void M_End(void *const context)
{
    ((FIXTURE *)context)->end_count++;
}

static void M_Render(void *const context)
{
    ((FIXTURE *)context)->renders++;
}

static void M_Flip(
    void *const context, const uint8_t drawing, const uint8_t visible)
{
    FIXTURE *const fixture = context;
    fixture->flips++;
    fixture->drawing = drawing;
    fixture->visible = visible;
}

static void M_Comment(void *const context, const uint8_t selection)
{
    FIXTURE *const fixture = context;
    CHECK_EQ_INT(selection, m_Room.playing);
    fixture->comments++;
}

static void M_Track(
    void *const context, const uint8_t selection, const uint8_t color)
{
    FIXTURE *const fixture = context;
    CHECK(fixture->tracks < 16);
    fixture->choices[fixture->tracks] = selection;
    fixture->colors[fixture->tracks++] = color;
}

static void M_RecordAudio(
    void *const context, const MUSIC_ROOM_AUDIO_REQUEST *const request)
{
    FIXTURE *const fixture = context;
    CHECK(fixture->audio_count < 8);
    fixture->old_playing[fixture->audio_count] = m_Room.playing;
    fixture->audio[fixture->audio_count++] = *request;
    CHECK(m_Room.midi_active == request->midi);
}

static void M_Init(FIXTURE *const fixture, const uint8_t playing)
{
    memset(fixture, 0, sizeof(*fixture));
    MusicRoom_Init();
    m_Room.playing = playing;
    fixture->view = (TEST_MUSIC_VIEW) { M_Begin, M_Track, M_Comment, M_Render,
                                        M_Flip,  M_End,   fixture };
    TestMusicRoom_Observe(&fixture->view);
    MusicRoom_Begin(nullptr, nullptr, false, false, M_RecordAudio, fixture);
}

TEST(release_gate_skips_blank_choice_and_keeps_held_keys_disarmed)
{
    FIXTURE fixture;
    M_Init(&fixture, 14);
    CHECK_EQ_INT(fixture.begin_count, 1);
    CHECK_EQ_INT(fixture.renders, 0);
    CHECK(MusicRoom_Update(GAME_KEY_DOWN).wait_frame);
    CHECK_EQ_INT(m_Room.selection, 14);
    CHECK_EQ_INT(fixture.visible, 1);
    CHECK_EQ_INT(fixture.drawing, 0);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_DOWN);
    CHECK_EQ_INT(m_Room.selection, 16);
    MusicRoom_Update(GAME_KEY_DOWN);
    CHECK_EQ_INT(m_Room.selection, 16);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_DOWN);
    CHECK_EQ_INT(m_Room.selection, 0);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_UP);
    CHECK_EQ_INT(m_Room.selection, 16);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_UP);
    CHECK_EQ_INT(m_Room.selection, 14);
    CHECK_EQ_INT(m_Room.playing, 14);
    CHECK_EQ_INT(fixture.audio_count, 0);
    CHECK_EQ_INT(fixture.renders, fixture.flips);
}

TEST(up_and_down_redraw_independently_in_source_order)
{
    FIXTURE fixture;
    M_Init(&fixture, 0);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_UP | GAME_KEY_DOWN);
    CHECK_EQ_INT(m_Room.selection, 0);
    const uint8_t choices[] = { 0, 16, 16, 0 }, colors[] = { 3, 15, 3, 15 };
    CHECK_EQ_INT(fixture.tracks, 4);
    CHECK(memcmp(fixture.choices, choices, sizeof(choices)) == 0);
    CHECK(memcmp(fixture.colors, colors, sizeof(colors)) == 0);
}

TEST(play_loads_both_profiles_before_updating_playing_and_restores_midi)
{
    for (int possible = 0; possible < 2; possible++) {
        for (int active = 0; active < 2; active++) {
            FIXTURE fixture;
            M_Init(&fixture, 0);
            m_Room.midi_possible = possible != 0;
            m_Room.midi_active = active != 0;
            MusicRoom_Update(0);
            MusicRoom_Update(GAME_KEY_DOWN);
            MusicRoom_Update(0);
            CHECK(MusicRoom_Update(GAME_KEY_OK).wait_frame);
            CHECK_EQ_INT(m_Room.playing, 1);
            CHECK(m_Room.midi_active == (active != 0));
            CHECK_EQ_INT(fixture.audio_count, 3);
            CHECK_EQ_INT(fixture.audio[0].action, MUSIC_ROOM_LOAD);
            CHECK(fixture.audio[0].midi == (possible != 0));
            CHECK_EQ_INT(fixture.audio[1].action, MUSIC_ROOM_LOAD);
            CHECK(!fixture.audio[1].midi);
            CHECK_EQ_INT(fixture.audio[2].action, MUSIC_ROOM_PLAY);
            CHECK(fixture.audio[2].midi == (active != 0));
            for (int event = 0; event < 3; event++) {
                CHECK_EQ_INT(fixture.audio[event].track, 1);
                CHECK_EQ_INT(fixture.old_playing[event], 0);
            }
            CHECK_EQ_INT(fixture.comments, 1);
            MusicRoom_Update(0);
            CHECK_EQ_INT(fixture.comments, 2);
            CHECK_EQ_INT(fixture.audio_count, 3);
        }
    }
}

TEST(play_and_cancel_finish_both_comments_before_waiting_for_release)
{
    FIXTURE fixture;
    M_Init(&fixture, 7);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_SHOT | GAME_KEY_CANCEL);
    CHECK_EQ_INT(fixture.comments, 1);
    const int before = fixture.renders;
    CHECK(MusicRoom_Update(0).done);
    CHECK_EQ_INT(fixture.comments, 2);
    CHECK_EQ_INT(fixture.renders, before);
    CHECK_EQ_INT(fixture.end_count, 1);
    const MUSIC_ROOM completed = m_Room;
    CHECK(MusicRoom_Update(0xffff).done);
    CHECK(memcmp(&m_Room, &completed, sizeof(completed)) == 0);
    CHECK_EQ_INT(fixture.end_count, 1);
    MusicRoom_Begin(nullptr, nullptr, false, false, M_RecordAudio, &fixture);
    CHECK_EQ_INT(m_Room.selection, 7);
    CHECK_EQ_INT(fixture.begin_count, 2);
}

TEST(new_cancel_during_a_play_wait_does_not_replace_the_original_mask)
{
    FIXTURE fixture;
    M_Init(&fixture, 0);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_SHOT);
    CHECK(MusicRoom_Update(GAME_KEY_CANCEL).wait_frame);
    CHECK_EQ_INT(fixture.comments, 2);
    CHECK_EQ_INT(fixture.end_count, 0);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_CANCEL);
    CHECK_EQ_INT(fixture.end_count, 0);
    CHECK(MusicRoom_Update(0).done);
}

TEST(quit_selection_never_loads_a_song_and_waits_for_release)
{
    MusicRoom_Init();
    TestMusicRoom_Observe(nullptr);
    MusicRoom_Begin(nullptr, nullptr, false, false, nullptr, nullptr);
    MusicRoom_Update(0);
    MusicRoom_Update(GAME_KEY_UP);
    MusicRoom_Update(0);
    CHECK_EQ_INT(m_Room.selection, MUSIC_ROOM_QUIT);
    CHECK(MusicRoom_Update(GAME_KEY_OK).wait_frame);
    CHECK_EQ_INT(m_Room.phase, MUSIC_ROOM_EXIT_RELEASE);
    CHECK(MusicRoom_Update(0).done);
    CHECK_EQ_INT(m_Room.playing, 0);
}
