#include "th02/wait.h"

#include <string.h>

static void M_Begin(WAIT *const wait, const WAIT_KIND kind)
{
    memset(wait, 0, sizeof(*wait));
    wait->kind = kind;
}

static void M_Frames(WAIT *const wait, const uint16_t frames)
{
    wait->count = frames;
    g_VSync.count1 = 0;
}

static bool M_Key(WAIT *const wait, const TH02_INPUT *const input)
{
    if (wait->key.phase == KEY_WAIT_DONE) {
        return true;
    }
    while (wait->key_frames < 2 && wait->key_frames < g_VSync.count1) {
        wait->key_frames++;
        if (GameInput_AdvanceWait(&wait->key, input)) {
            return true;
        }
    }
    if (wait->key_frames == 2) {
        wait->key_frames = 0;
        M_Frames(wait, 2);
    }
    return false;
}

void Wait_Cancel(WAIT *const wait)
{
    wait->kind = WAIT_READY;
}

void Wait_Delay(WAIT *const wait, const uint16_t frames)
{
    M_Begin(wait, WAIT_DELAY);
    M_Frames(wait, frames);
}

void Wait_Edge(WAIT *const wait)
{
    M_Begin(wait, WAIT_EDGE);
    wait->count = g_VSync.count1;
}

void Wait_Key(WAIT *const wait, const TH02_INPUT *const input)
{
    M_Begin(wait, WAIT_KEY);
    GameInput_BeginWait(&wait->key, input);
    M_Frames(wait, 2);
}

void Wait_Fade(
    WAIT *const wait, const PALETTE_FADE_KIND kind, const int16_t speed)
{
    M_Begin(wait, WAIT_FADE);
    Palette_BeginFade(&wait->fade, kind, speed);
    wait->count = g_VSync.count1;
}

void Wait_Measure(WAIT *const wait, const uint16_t measure, const bool music)
{
    M_Begin(wait, WAIT_MEASURE);
    wait->measure = measure;
    wait->music = music;
    M_Frames(wait, music ? 1 : 100);
}

bool Wait_Ready(
    WAIT *const wait, const TH02_INPUT *const input, const uint16_t measure)
{
    switch (wait->kind) {
    case WAIT_READY:
        return true;
    case WAIT_DELAY:
        return g_VSync.count1 >= wait->count;
    case WAIT_EDGE:
        return g_VSync.count1 != wait->count;
    case WAIT_KEY:
        return M_Key(wait, input);
    case WAIT_FADE:
        if (g_VSync.count1 == wait->count) {
            return false;
        }
        if (Palette_FadeVSync(&wait->fade)) {
            return true;
        }
        wait->count = g_VSync.count1;
        return false;
    case WAIT_MEASURE:
        if (g_VSync.count1 < wait->count) {
            return false;
        }
        if (wait->music && measure < wait->measure) {
            M_Frames(wait, 1);
            return false;
        }
        wait->kind = WAIT_DELAY;
        return true;
    }
    return false;
}
