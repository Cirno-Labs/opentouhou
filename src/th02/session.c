#include "th02/session.h"

#include <string.h>

typedef struct {
    bool extra;
    bool active;
} GAME_START;

static GAME_START m_Start;

void Session_CancelStart(void)
{
    memset(&m_Start, 0, sizeof(m_Start));
}

void Session_Init(TH02_SESSION *const session)
{
    memset(session, 0, sizeof(*session));
    Session_CancelStart();
}

SESSION_EVENTS Session_BeginStart(const bool extra)
{
    Session_CancelStart();
    m_Start.extra = extra;
    m_Start.active = true;
    return (SESSION_EVENTS) { .force_effect = SOUND_EFFECT_ITEM,
                              .wait_frames = 20 };
}

SESSION_EVENTS Session_FinishStart(
    TH02_SESSION *const session, const TH02_SETTINGS *const settings)
{
    SESSION_EVENTS events = { .force_effect = SOUND_EFFECT_NONE };
    if (!m_Start.active) {
        return events;
    }
    session->lives = settings->lives;
    session->bombs = settings->bombs;
    session->start_lives = settings->lives;
    session->start_bombs = settings->bombs;
    session->music = settings->music;
    session->start_power = 0;
    session->score = 0;
    session->continues_used = 0;
    session->unused_3 = 0;
    session->unused_1 = 0;
    session->demo_num = 0;
    session->highest_score = 0;
    session->rank = m_Start.extra ? SESSION_EXTRA_RANK : settings->difficulty;
    session->stage = m_Start.extra ? 5 : 0;
    if (m_Start.extra) {
        session->lives = session->start_lives = 2;
        session->bombs = session->start_bombs = 1;
    }
    Session_CancelStart();
    events.save_settings = events.select_shot = true;
    return events;
}

SESSION_EVENTS Session_BeginDemo(
    TH02_SESSION *const session, const TH02_SETTINGS *const settings,
    const uint8_t demo)
{
    session->lives = session->start_lives = 2;
    session->bombs = session->start_bombs = 3;
    session->music = settings->music;
    session->rank = TH02_DIFFICULTY_NORMAL;
    session->continues_used = 0;
    session->unused_3 = 0;
    session->demo_num = (int8_t)demo;
    session->shot_type = 0;
    return (SESSION_EVENTS) {
        .force_effect = SOUND_EFFECT_NONE,
        .save_settings = true,
        .enter_main = true,
    };
}

void Session_EnterMain(
    const TH02_SESSION *const session, SESSION_ENTRY *const entry)
{
    memset(entry, 0, sizeof(*entry));
    entry->stage = (int8_t)session->stage;
    entry->lives = (int8_t)session->start_lives;
    entry->bombs = (int8_t)session->start_bombs;
    entry->rank = session->rank;
    entry->power = (uint8_t)session->start_power;
    if (entry->power == 0) {
        entry->power = 1;
    }
}
