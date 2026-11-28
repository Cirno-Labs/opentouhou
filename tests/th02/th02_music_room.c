#include "tests/th02/th02_music_room.h"

static const TEST_MUSIC_VIEW *m_MusicRoomObserver;

static void M_MusicRoomBegin(
    const MUSIC_ROOM_ASSETS *assets, const TITLE_ASSETS *menu,
    uint8_t selection)
{
    if (m_MusicRoomObserver != nullptr
        && m_MusicRoomObserver->begin != nullptr) {
        m_MusicRoomObserver->begin(m_MusicRoomObserver->context, selection);
    }
}

static void M_MusicRoomTrack(uint8_t selection, uint8_t color)
{
    if (m_MusicRoomObserver != nullptr
        && m_MusicRoomObserver->track != nullptr) {
        m_MusicRoomObserver->track(
            m_MusicRoomObserver->context, selection, color);
    }
}

static void M_MusicRoomComment(uint8_t selection)
{
    if (m_MusicRoomObserver != nullptr
        && m_MusicRoomObserver->comment != nullptr) {
        m_MusicRoomObserver->comment(m_MusicRoomObserver->context, selection);
    }
}

static void M_MusicRoomFrame(void)
{
    if (m_MusicRoomObserver != nullptr
        && m_MusicRoomObserver->frame != nullptr) {
        m_MusicRoomObserver->frame(m_MusicRoomObserver->context);
    }
}

static void M_MusicRoomFlip(uint8_t drawing, uint8_t visible)
{
    if (m_MusicRoomObserver != nullptr
        && m_MusicRoomObserver->flip != nullptr) {
        m_MusicRoomObserver->flip(
            m_MusicRoomObserver->context, drawing, visible);
    }
}

static void M_MusicRoomEnd(void)
{
    if (m_MusicRoomObserver != nullptr && m_MusicRoomObserver->end != nullptr) {
        m_MusicRoomObserver->end(m_MusicRoomObserver->context);
    }
}

const MUSIC_ROOM_RENDER g_TestMusicRoomRender = {
    .begin = M_MusicRoomBegin,
    .track = M_MusicRoomTrack,
    .comment = M_MusicRoomComment,
    .frame = M_MusicRoomFrame,
    .flip = M_MusicRoomFlip,
    .end = M_MusicRoomEnd,
};

void TestMusicRoom_Observe(const TEST_MUSIC_VIEW *view)
{
    m_MusicRoomObserver = view;
}
