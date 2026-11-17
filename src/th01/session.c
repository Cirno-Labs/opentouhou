#include "th01/session.h"

void Session_Begin(
    SESSION *const session, const SESSION_BEGIN begin,
    const TH01_SETTINGS *const settings, const uint32_t frame_rand)
{
    if (begin == SESSION_START) {
        *session = (SESSION) {};
    }
    session->settings = *settings;
    session->frame_rand = frame_rand;
    session->exit_rand = frame_rand;
    session->score = 0;
    session->lives = (int16_t)(settings->lives_extra + 2);
    session->bombs = (int16_t)settings->bombs;
    session->pellet_speed = -4;
    session->point_value = 0;
}

uint8_t Session_Scene(const SESSION *const session)
{
    return session->stage_id < 5
        ? 0
        : (uint8_t)((session->stage_id / 5) * 2 - 1 + session->route);
}
