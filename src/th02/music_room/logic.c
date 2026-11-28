#include "th02/music_room/logic.h"

#include "th02/input.h"
#include "th02/music_room/private.h"

#include "core/base/assert.h"

#include <string.h>

static MUSIC_ROOM m_Room;
MUSIC_ROOM *const p_Room = &m_Room;
static const MUSIC_ROOM_RENDER *m_Render;

static MUSIC_ROOM_EVENTS M_Animate(void)
{
    m_Render->frame();
    const uint8_t visible_page = m_Room.next_visible_page;
    m_Room.next_visible_page ^= 1;
    m_Render->flip(m_Room.next_visible_page, visible_page);
    return (MUSIC_ROOM_EVENTS) { .wait_frame = true };
}

static void M_Audio(const MUSIC_ROOM_AUDIO_ACTION action)
{
    if (m_Room.audio != nullptr) {
        const MUSIC_ROOM_AUDIO_REQUEST request = { action, m_Room.selection,
                                                   m_Room.midi_active };
        m_Room.audio(m_Room.audio_context, &request);
    }
}

static void M_Play(void)
{
    const bool midi_active = m_Room.midi_active;
    m_Room.midi_active = m_Room.midi_possible;
    M_Audio(MUSIC_ROOM_LOAD);
    m_Room.midi_active = false;
    M_Audio(MUSIC_ROOM_LOAD);
    m_Room.midi_active = midi_active;
    M_Audio(MUSIC_ROOM_PLAY);
    m_Room.playing = m_Room.selection;
}

void MusicRoom_Init(const MUSIC_ROOM_RENDER *const render)
{
    memset(&m_Room, 0, sizeof(m_Room));
    m_Render = render;
    MusicRoomRender_Init();
}

void MusicRoom_Reset(void)
{
    const uint8_t playing = m_Room.playing;
    memset(&m_Room, 0, sizeof(m_Room));
    m_Room.playing = playing;
}

void MusicRoom_Begin(
    const MUSIC_ROOM_ASSETS *const assets, const TITLE_ASSETS *const menu,
    const bool midi_possible, const bool midi_active,
    const MUSIC_ROOM_AUDIO audio, void *const audio_context)
{
    ASSERT(m_Room.playing < MUSIC_ROOM_TRACK_COUNT);
    const uint8_t playing = m_Room.playing;
    memset(&m_Room, 0, sizeof(m_Room));
    m_Room.playing = playing;
    m_Room.midi_possible = midi_possible;
    m_Room.midi_active = midi_active;
    m_Room.audio = audio;
    m_Room.audio_context = audio_context;
    m_Room.selection = m_Room.playing;
    m_Room.next_visible_page = 1;
    m_Render->begin(assets, menu, m_Room.selection);
}

MUSIC_ROOM_EVENTS MusicRoom_Update(const uint16_t keys)
{
    if (m_Room.phase == MUSIC_ROOM_DONE) {
        return (MUSIC_ROOM_EVENTS) { .done = true };
    }
    if (m_Room.phase == MUSIC_ROOM_COMMENT) {
        m_Render->comment(m_Room.selection);
        m_Room.phase = (m_Room.pending_keys & GAME_KEY_CANCEL) != 0
            ? MUSIC_ROOM_EXIT_RELEASE
            : MUSIC_ROOM_RELEASE;
    }
    for (;;) {
        if (m_Room.phase == MUSIC_ROOM_EXIT_RELEASE) {
            if (keys != 0) {
                return M_Animate();
            }
            m_Render->end();
            m_Room.phase = MUSIC_ROOM_DONE;
            return (MUSIC_ROOM_EVENTS) { .done = true };
        }
        if (m_Room.phase == MUSIC_ROOM_RELEASE) {
            if (keys != 0) {
                return M_Animate();
            }
            m_Room.phase = MUSIC_ROOM_CONTROLS;
        }
        if ((keys & GAME_KEY_UP) != 0) {
            m_Render->track(m_Room.selection, 3);
            m_Room.selection =
                m_Room.selection > 0 ? m_Room.selection - 1 : MUSIC_ROOM_QUIT;
            if (m_Room.selection == MUSIC_ROOM_TRACK_COUNT) {
                m_Room.selection--;
            }
            m_Render->track(m_Room.selection, 15);
        }
        if ((keys & GAME_KEY_DOWN) != 0) {
            m_Render->track(m_Room.selection, 3);
            m_Room.selection =
                m_Room.selection < MUSIC_ROOM_QUIT ? m_Room.selection + 1 : 0;
            if (m_Room.selection == MUSIC_ROOM_TRACK_COUNT) {
                m_Room.selection++;
            }
            m_Render->track(m_Room.selection, 15);
        }
        if ((keys & (GAME_KEY_SHOT | GAME_KEY_OK)) != 0) {
            if (m_Room.selection == MUSIC_ROOM_QUIT) {
                m_Room.phase = MUSIC_ROOM_EXIT_RELEASE;
                continue;
            }
            M_Play();
            m_Render->comment(m_Room.selection);
            m_Room.pending_keys = keys;
            m_Room.phase = MUSIC_ROOM_COMMENT;
            return M_Animate();
        }
        if ((keys & GAME_KEY_CANCEL) != 0) {
            m_Room.phase = MUSIC_ROOM_EXIT_RELEASE;
            continue;
        }
        if (keys == 0) {
            return M_Animate();
        }
        m_Room.phase = MUSIC_ROOM_RELEASE;
    }
}
