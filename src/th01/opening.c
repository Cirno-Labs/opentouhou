#include "th01/opening.h"

#include "pc98/zun/master_rng.h"

#include "core/base/assert.h"

#include <stdio.h>
#include <string.h>

#include "th01/opening_strings.h"

static const char *const m_TrackFiles[] = {
    "reimu.mdt", "ZIPANGU.mdt", "st0.mdt",    "st1.mdt",      "st2.mdt",
    "st3.mdt",   "st4.mdt",     "st5.mdt",    "positive.mdt", "legend.mdt",
    "kami.mdt",  "tensi.mdt",   "syugen.mdt", "alice.mdt",    "iris.mdt",
};

OPENING g_Opening;
static const OPENING_ASSETS *m_Assets;

static void M_Image(const PI_IMAGE *const image, const int32_t key)
{
    const VIDEO_IMAGE view = {
        .pixels = image->pixels,
        .width = image->width,
        .height = image->height,
        .stride = image->width,
        .key = key,
    };
    Video_Blit(&view, (GFX_RECT) { 0, 0, image->width, image->height }, 0, 0);
}

static void M_Text(
    const char *const text, const int32_t left, const int32_t top,
    const uint8_t color, const GAME_TEXT_WEIGHT weight)
{
    const GAME_TEXT_STYLE style = { .weight = weight };
    GameText_Draw(&m_Assets->font, text, left, top, color, &style);
}

static int32_t M_Top(const int32_t count)
{
    return 316 - count * 10;
}

static void M_Erase(const int32_t count)
{
    Video_Restore16((GFX_RECT) { 220, M_Top(count), 176, count * 20 });
}

static void M_Choice(const int32_t choice, const uint8_t color)
{
    if (g_Opening.phase == OPENING_MAIN) {
        M_Text(
            m_MainMenuItems[choice], 244, M_Top(4) + choice * 20, color,
            GAME_TEXT_BLACK);
        return;
    }
    const bool music = g_Opening.phase == OPENING_MUSIC;
    const int32_t top = music ? M_Top(3) + choice * 40 : M_Top(5) + choice * 20;
    Video_Restore16((GFX_RECT) { 228, top, 176, 16 });
    char text[256];
    if (music) {
        if (choice == 0) {
            Video_Restore16((GFX_RECT) { 228, top + 20, 192, 16 });
            snprintf(
                text, sizeof(text), "%s%.2d", m_MusicMenuItems[0],
                g_Opening.track);
            M_Text(text, 228, top, color, GAME_TEXT_BLACK);
            M_Text(
                m_TrackTitles[g_Opening.track], 228, top + 20, color,
                GAME_TEXT_BLACK);
        } else {
            M_Text(m_MusicMenuItems[1], 228, top, color, GAME_TEXT_BLACK);
        }
        return;
    }
    const char *value = "";
    if (choice == 0) {
        value = m_Ranks[g_Settings.difficulty];
    } else if (choice == 1) {
        value = m_MusicModes[g_Settings.music];
    } else if (choice == 2) {
        value = m_Lives[g_Settings.lives_extra];
    }
    snprintf(text, sizeof(text), "%s%s", m_OptionMenuItems[choice], value);
    M_Text(text, 228, top, color, GAME_TEXT_BLACK);
}

static void M_Lines(void)
{
    for (int32_t i = 0; i < 2; i++) {
        Video_Restore16((GFX_RECT) { 0, g_Opening.line_y[i], VIDEO_WIDTH, 1 });
    }
    for (int32_t i = 0; i < 2; i++) {
        int32_t y;
        do {
            y = MasterRng_Next(&g_Opening.random) % VIDEO_HEIGHT;
        } while (g_Opening.drawn_rows[y]);
        g_Opening.drawn_rows[y] = true;
        g_Opening.line_y[i] = y;
        Video_HorizontalLine(0, VIDEO_WIDTH - 1, y, 15);
    }
    g_Opening.line_count++;
}

static void M_Window(void)
{
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    M_Image(&m_Assets->window, 15);
    Palette_Import(&g_Opening.palette.resource, m_Assets->window.palette);
    Video_Copy(0, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 1, 0, 0);
    g_Opening.random = g_Opening.frame_rand;
    g_Opening.phase = OPENING_MAIN;
    g_Opening.entering = true;
}

static int32_t M_Ring(
    const int32_t value, const int32_t delta, const int32_t count)
{
    return (value + delta + count) % count;
}

static void M_Horizontal(const int32_t delta)
{
    if (g_Opening.phase == OPENING_MUSIC) {
        if (g_Opening.selection == 0) {
            g_Opening.track =
                M_Ring(g_Opening.track, delta, OPENING_TRACK_COUNT);
        }
    } else if (g_Opening.selection == 0) {
        g_Settings.difficulty = M_Ring(g_Settings.difficulty, delta, 4);
    } else if (g_Opening.selection == 1) {
        g_Settings.music = M_Ring(g_Settings.music, delta, 2);
    } else if (g_Opening.selection == 2) {
        g_Settings.lives_extra = M_Ring(g_Settings.lives_extra, delta, 5);
    }
    M_Choice(g_Opening.selection, 15);
}

static void M_Menu(const TH01_INPUT *const raw, OPENING_EVENTS *const events)
{
    const OPENING_PHASE phase = g_Opening.phase;
    const bool main = phase == OPENING_MAIN;
    const bool music = phase == OPENING_MUSIC;
    const int32_t count = main ? 4 : (music ? 2 : 5);
    const int32_t previous = g_Opening.selection;
    // Discard entry selection after sensing, including the stale music exit 3.
    g_Opening.selection = GameInput_SenseMenu(
        &g_Opening.input, raw, main ? MENU_INPUT_MAIN : MENU_INPUT_OPTIONS,
        main ? 4 : g_Opening.option_item_count,
        g_Opening.entering && !main ? 0 : g_Opening.selection);
    if (g_Opening.entering) {
        g_Opening.entering = false;
        if (!main) {
            g_Opening.selection = 0;
            g_Opening.option_item_count = count;
        }
        if (music) {
            g_Opening.input.ok = false;
            g_Opening.input.shot = false;
        }
        M_Erase(phase == OPENING_OPTIONS ? 4 : 5);
        for (int32_t i = 0; i < count; i++) {
            M_Choice(i, 5);
        }
        M_Choice(g_Opening.selection, 15);
    } else if (previous != g_Opening.selection) {
        M_Choice(previous, 5);
        M_Choice(g_Opening.selection, 15);
    }
    if (!main) {
        const int32_t slot = music ? 1 : 0;
        if (g_Opening.input.left && !g_Opening.left_locked[slot]) {
            M_Horizontal(-1);
        }
        g_Opening.left_locked[slot] = g_Opening.input.left;
        if (g_Opening.input.right && !g_Opening.right_locked[slot]) {
            M_Horizontal(1);
        }
        g_Opening.right_locked[slot] = g_Opening.input.right;
    }
    const bool confirm = g_Opening.input.ok || g_Opening.input.shot;
    if (main) {
        if (confirm) {
            if (g_Opening.selection < 2) {
                g_Opening.exit_action = g_Opening.selection == 0
                    ? OPENING_ACTION_START
                    : OPENING_ACTION_CONTINUE;
                g_Opening.phase = OPENING_EXIT_FADE;
                Palette_BeginFade(
                    &g_Opening.fade, &g_Opening.palette,
                    PALETTE_FADE_BLACK_OUT);
                events->audio = OPENING_AUDIO_FADE;
                return;
            } else if (g_Opening.selection == 2) {
                g_Opening.phase = OPENING_OPTIONS;
                g_Opening.entering = true;
            } else {
                events->action = OPENING_ACTION_QUIT;
            }
        }
        if (g_Opening.input.cancel) {
            events->action = OPENING_ACTION_QUIT;
        }
    } else if (!music) {
        if ((confirm && g_Opening.selection == 4) || g_Opening.input.cancel) {
            g_Opening.phase = OPENING_WAIT_MAIN;
            g_Opening.entering = true;
            g_Opening.selection = 2;
        }
        if (confirm && g_Opening.selection == 3) {
            g_Opening.phase = OPENING_MUSIC;
            g_Opening.entering = true;
            g_Opening.selection = 0;
        }
    } else {
        if ((confirm && g_Opening.selection == 1) || g_Opening.input.cancel) {
            g_Opening.phase = OPENING_WAIT_OPTIONS;
            g_Opening.entering = true;
            g_Opening.selection = 3;
        }
        const bool held = g_Settings.fix_debouncing && g_Opening.confirm_locked;
        if (confirm && g_Opening.selection == 0 && !held) {
            events->audio = OPENING_AUDIO_PLAY;
            events->track = g_Opening.track;
            g_Opening.track_starts++;
        }
        g_Opening.confirm_locked = confirm;
    }
    events->clear_key_buffer = true;
    g_Opening.frame_rand++;
}

const char *Opening_TrackFile(const int32_t track)
{
    ASSERT(track >= 0 && track < OPENING_TRACK_COUNT);
    return m_TrackFiles[track];
}

OPENING_EVENTS Opening_Init(const OPENING_ASSETS *const assets)
{
    memset(&g_Opening, 0, sizeof(g_Opening));
    m_Assets = assets;
    g_Opening.phase = OPENING_FADE;
    g_Opening.random = 1;
    g_Opening.option_item_count = 4;
    g_Opening.music_mode = g_Settings.music;
    Palette_Init(&g_Opening.palette);
    Video_SelectDrawingPage(1);
    Video_SelectVisiblePage(0);
    M_Image(&assets->title, -1);
    Palette_Import(&g_Opening.palette.resource, assets->title.palette);
    Palette_Set(&g_Opening.palette, &g_Opening.palette.resource);
    Palette_ShowSolid(0, 0, 0);
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    M_Image(&assets->revealed, -1);
    Palette_Import(&g_Opening.palette.resource, assets->revealed.palette);
    Video_SelectDrawingPage(0);
    Palette_BeginFade(
        &g_Opening.fade, &g_Opening.palette, PALETTE_FADE_BLACK_IN);
    return (OPENING_EVENTS) { .audio = OPENING_AUDIO_PLAY,
                              .track = 0,
                              .clear_key_buffer = true };
}

OPENING_EVENTS Opening_Tick(const TH01_INPUT *const raw, const bool any_key)
{
    OPENING_EVENTS events = {};
    if (g_Opening.phase == OPENING_EXIT_FADE) {
        Palette_FadeStep(&g_Opening.fade, &g_Opening.palette);
        if (g_Opening.fade.remaining == 0) {
            events.action = g_Opening.exit_action;
        }
        return events;
    }
    g_Opening.count1++;
    if (g_Opening.phase == OPENING_FADE) {
        const bool stepped =
            Palette_FadeStep(&g_Opening.fade, &g_Opening.palette);
        ASSERT(stepped);
        if (g_Opening.fade.remaining == 0) {
            g_Opening.phase = OPENING_HOLD;
        }
        return events;
    }
    if (g_Opening.phase == OPENING_HOLD) {
        if (g_Opening.count1 < 100) {
            return events;
        }
        g_Opening.count1 = 0;
        g_Opening.phase = OPENING_LINES;
        M_Lines();
        return events;
    }
    if (g_Opening.phase == OPENING_LINES) {
        g_Opening.count1 = 0;
        if (g_Opening.line_count < 100) {
            M_Lines();
            return events;
        }
        Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
        g_Opening.phase = OPENING_PROMPT;
    }
    if (g_Opening.phase == OPENING_PROMPT) {
        if (!any_key) {
            if (g_Opening.count1 != 0) {
                g_Opening.count1 = 0;
                if (g_Opening.prompt_frame % 70 < 50) {
                    M_Text(m_Prompt, 244, M_Top(1), 15, GAME_TEXT_BOLD);
                } else {
                    Video_Restore16((GFX_RECT) { 244, M_Top(1), 120, 16 });
                }
                g_Opening.prompt_frame = (g_Opening.prompt_frame + 1) % 70;
            }
            return events;
        }
        M_Window();
    }
    if (g_Opening.phase == OPENING_WAIT_MAIN
        || g_Opening.phase == OPENING_WAIT_OPTIONS) {
        if (g_Opening.wait == 0) {
            if (g_Opening.phase == OPENING_WAIT_MAIN
                && g_Settings.music != g_Opening.music_mode) {
                g_Opening.music_mode = g_Settings.music;
                events.audio = g_Opening.music_mode == SETTINGS_MUSIC_OFF
                    ? OPENING_AUDIO_STOP
                    : OPENING_AUDIO_PLAY;
                events.track = 0;
            }
            g_Opening.wait = 15;
        }
        if (--g_Opening.wait == 0) {
            g_Opening.phase = g_Opening.phase == OPENING_WAIT_MAIN
                ? OPENING_MAIN
                : OPENING_OPTIONS;
            g_Opening.frame_rand++;
            g_Opening.count1 = 0;
            events.clear_key_buffer = true;
        }
        return events;
    }
    if (g_Opening.phase == OPENING_QUIT) {
        return events;
    }
    g_Opening.count1 = 0;
    M_Menu(raw, &events);
    if (events.action == OPENING_ACTION_QUIT) {
        g_Opening.phase = OPENING_QUIT;
        events.audio = OPENING_AUDIO_STOP;
    }
    return events;
}

void Opening_Close(void)
{
    memset(&g_Opening, 0, sizeof(g_Opening));
    m_Assets = nullptr;
}
