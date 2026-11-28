#include "th01/opening/opening.h"

#include "th01/opening/strings.h"
#include "th01/palette.h"
#include "th01/settings.h"

#include "core/base/assert.h"
#include "pc98/zun/master_rng.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    PALETTE_STATE palette;
    PALETTE_FADE fade;
    MENU_INPUT input;
    OPENING_ACTION exit_action;
    int32_t line_count;
    int32_t line_y[2];
    int32_t prompt_frame;
    int32_t selection;
    int32_t option_item_count;
    int32_t music_mode;
    int32_t wait;
    bool drawn_rows[VIDEO_HEIGHT];
    bool entering;
    bool left_locked[2];
    bool right_locked[2];
    bool confirm_locked;
} OPENING_MENU;

static OPENING m_Opening;
const OPENING *const g_Opening = &m_Opening;
static OPENING_MENU m_Menu;
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
    if (m_Opening.phase == OPENING_MAIN) {
        M_Text(
            g_OpeningMainMenuItems[choice], 244, M_Top(4) + choice * 20, color,
            GAME_TEXT_BLACK);
        return;
    }
    const bool music = m_Opening.phase == OPENING_MUSIC;
    const int32_t top = music ? M_Top(3) + choice * 40 : M_Top(5) + choice * 20;
    Video_Restore16((GFX_RECT) { 228, top, 176, 16 });
    char text[256];
    if (music) {
        if (choice == 0) {
            Video_Restore16((GFX_RECT) { 228, top + 20, 192, 16 });
            snprintf(
                text, sizeof(text), "%s%.2d", g_OpeningMusicMenuItems[0],
                m_Opening.track);
            M_Text(text, 228, top, color, GAME_TEXT_BLACK);
            M_Text(
                g_OpeningTrackTitles[m_Opening.track], 228, top + 20, color,
                GAME_TEXT_BLACK);
        } else {
            M_Text(
                g_OpeningMusicMenuItems[1], 228, top, color, GAME_TEXT_BLACK);
        }
        return;
    }
    const char *value = "";
    if (choice == 0) {
        value = g_OpeningRanks[g_Settings.difficulty];
    } else if (choice == 1) {
        value = g_OpeningMusicModes[g_Settings.music];
    } else if (choice == 2) {
        value = g_OpeningLives[g_Settings.lives_extra];
    }
    snprintf(
        text, sizeof(text), "%s%s", g_OpeningOptionMenuItems[choice], value);
    M_Text(text, 228, top, color, GAME_TEXT_BLACK);
}

static void M_Lines(void)
{
    for (int32_t i = 0; i < 2; i++) {
        Video_Restore16((GFX_RECT) { 0, m_Menu.line_y[i], VIDEO_WIDTH, 1 });
    }
    for (int32_t i = 0; i < 2; i++) {
        int32_t y;
        do {
            y = MasterRng_Next(&m_Opening.random) % VIDEO_HEIGHT;
        } while (m_Menu.drawn_rows[y]);
        m_Menu.drawn_rows[y] = true;
        m_Menu.line_y[i] = y;
        Video_HorizontalLine(0, VIDEO_WIDTH - 1, y, 15);
    }
    m_Menu.line_count++;
}

static void M_Window(void)
{
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    M_Image(&m_Assets->window, 15);
    Palette_Import(&m_Menu.palette.resource, m_Assets->window.palette);
    Video_Copy(0, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 1, 0, 0);
    m_Opening.random = m_Opening.frame_rand;
    m_Opening.phase = OPENING_MAIN;
    m_Menu.entering = true;
}

static int32_t M_Ring(
    const int32_t value, const int32_t delta, const int32_t count)
{
    return (value + delta + count) % count;
}

static void M_Horizontal(const int32_t delta)
{
    if (m_Opening.phase == OPENING_MUSIC) {
        if (m_Menu.selection == 0) {
            m_Opening.track =
                M_Ring(m_Opening.track, delta, OPENING_TRACK_COUNT);
        }
    } else if (m_Menu.selection == 0) {
        g_Settings.difficulty = M_Ring(g_Settings.difficulty, delta, 4);
    } else if (m_Menu.selection == 1) {
        g_Settings.music = M_Ring(g_Settings.music, delta, 2);
    } else if (m_Menu.selection == 2) {
        g_Settings.lives_extra = M_Ring(g_Settings.lives_extra, delta, 5);
    }
    M_Choice(m_Menu.selection, 15);
}

static void M_Menu(const TH01_INPUT *const raw, OPENING_EVENTS *const events)
{
    const OPENING_PHASE phase = m_Opening.phase;
    const bool main = phase == OPENING_MAIN;
    const bool music = phase == OPENING_MUSIC;
    const int32_t count = main ? 4 : (music ? 2 : 5);
    const int32_t previous = m_Menu.selection;
    // Discard entry selection after sensing, including the stale music exit 3.
    m_Menu.selection = GameInput_SenseMenu(
        &m_Menu.input, raw, main ? MENU_INPUT_MAIN : MENU_INPUT_OPTIONS,
        main ? 4 : m_Menu.option_item_count,
        m_Menu.entering && !main ? 0 : m_Menu.selection);
    if (m_Menu.entering) {
        m_Menu.entering = false;
        if (!main) {
            m_Menu.selection = 0;
            m_Menu.option_item_count = count;
        }
        if (music) {
            m_Menu.input.ok = false;
            m_Menu.input.shot = false;
        }
        M_Erase(phase == OPENING_OPTIONS ? 4 : 5);
        for (int32_t i = 0; i < count; i++) {
            M_Choice(i, 5);
        }
        M_Choice(m_Menu.selection, 15);
    } else if (previous != m_Menu.selection) {
        M_Choice(previous, 5);
        M_Choice(m_Menu.selection, 15);
    }
    if (!main) {
        const int32_t slot = music ? 1 : 0;
        if (m_Menu.input.left && !m_Menu.left_locked[slot]) {
            M_Horizontal(-1);
        }
        m_Menu.left_locked[slot] = m_Menu.input.left;
        if (m_Menu.input.right && !m_Menu.right_locked[slot]) {
            M_Horizontal(1);
        }
        m_Menu.right_locked[slot] = m_Menu.input.right;
    }
    const bool confirm = m_Menu.input.ok || m_Menu.input.shot;
    if (main) {
        if (confirm) {
            if (m_Menu.selection < 2) {
                m_Menu.exit_action = m_Menu.selection == 0
                    ? OPENING_ACTION_START
                    : OPENING_ACTION_CONTINUE;
                m_Opening.phase = OPENING_EXIT_FADE;
                Palette_BeginFade(
                    &m_Menu.fade, &m_Menu.palette, PALETTE_FADE_BLACK_OUT);
                events->audio = OPENING_AUDIO_FADE;
                return;
            } else if (m_Menu.selection == 2) {
                m_Opening.phase = OPENING_OPTIONS;
                m_Menu.entering = true;
            } else {
                events->action = OPENING_ACTION_QUIT;
            }
        }
        if (m_Menu.input.cancel) {
            events->action = OPENING_ACTION_QUIT;
        }
    } else if (!music) {
        if ((confirm && m_Menu.selection == 4) || m_Menu.input.cancel) {
            m_Opening.phase = OPENING_WAIT_MAIN;
            m_Menu.entering = true;
            m_Menu.selection = 2;
        }
        if (confirm && m_Menu.selection == 3) {
            m_Opening.phase = OPENING_MUSIC;
            m_Menu.entering = true;
            m_Menu.selection = 0;
        }
    } else {
        if ((confirm && m_Menu.selection == 1) || m_Menu.input.cancel) {
            m_Opening.phase = OPENING_WAIT_OPTIONS;
            m_Menu.entering = true;
            m_Menu.selection = 3;
        }
        const bool held = g_Settings.fix_debouncing && m_Menu.confirm_locked;
        if (confirm && m_Menu.selection == 0 && !held) {
            events->audio = OPENING_AUDIO_PLAY;
            events->track = m_Opening.track;
            m_Opening.track_starts++;
        }
        m_Menu.confirm_locked = confirm;
    }
    events->clear_key_buffer = true;
    m_Opening.frame_rand++;
}

OPENING_EVENTS Opening_Init(const OPENING_ASSETS *const assets)
{
    memset(&m_Opening, 0, sizeof(m_Opening));
    memset(&m_Menu, 0, sizeof(m_Menu));
    m_Assets = assets;
    m_Opening.phase = OPENING_FADE;
    m_Opening.random = 1;
    m_Menu.option_item_count = 4;
    m_Menu.music_mode = g_Settings.music;
    Palette_Init(&m_Menu.palette);
    Video_SelectDrawingPage(1);
    Video_SelectVisiblePage(0);
    M_Image(&assets->title, -1);
    Palette_Import(&m_Menu.palette.resource, assets->title.palette);
    Palette_Set(&m_Menu.palette, &m_Menu.palette.resource);
    Palette_ShowSolid(0, 0, 0);
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    M_Image(&assets->revealed, -1);
    Palette_Import(&m_Menu.palette.resource, assets->revealed.palette);
    Video_SelectDrawingPage(0);
    Palette_BeginFade(&m_Menu.fade, &m_Menu.palette, PALETTE_FADE_BLACK_IN);
    return (OPENING_EVENTS) { .audio = OPENING_AUDIO_PLAY,
                              .track = 0,
                              .clear_key_buffer = true };
}

OPENING_EVENTS Opening_Tick(const TH01_INPUT *const raw, const bool any_key)
{
    OPENING_EVENTS events = {};
    if (m_Opening.phase == OPENING_EXIT_FADE) {
        Palette_FadeStep(&m_Menu.fade, &m_Menu.palette);
        if (m_Menu.fade.remaining == 0) {
            events.action = m_Menu.exit_action;
        }
        return events;
    }
    m_Opening.count1++;
    if (m_Opening.phase == OPENING_FADE) {
        const bool stepped = Palette_FadeStep(&m_Menu.fade, &m_Menu.palette);
        ASSERT(stepped);
        if (m_Menu.fade.remaining == 0) {
            m_Opening.phase = OPENING_HOLD;
        }
        return events;
    }
    if (m_Opening.phase == OPENING_HOLD) {
        if (m_Opening.count1 < 100) {
            return events;
        }
        m_Opening.count1 = 0;
        m_Opening.phase = OPENING_LINES;
        M_Lines();
        return events;
    }
    if (m_Opening.phase == OPENING_LINES) {
        m_Opening.count1 = 0;
        if (m_Menu.line_count < 100) {
            M_Lines();
            return events;
        }
        Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
        m_Opening.phase = OPENING_PROMPT;
    }
    if (m_Opening.phase == OPENING_PROMPT) {
        if (!any_key) {
            if (m_Opening.count1 != 0) {
                m_Opening.count1 = 0;
                if (m_Menu.prompt_frame % 70 < 50) {
                    M_Text(g_OpeningPrompt, 244, M_Top(1), 15, GAME_TEXT_BOLD);
                } else {
                    Video_Restore16((GFX_RECT) { 244, M_Top(1), 120, 16 });
                }
                m_Menu.prompt_frame = (m_Menu.prompt_frame + 1) % 70;
            }
            return events;
        }
        M_Window();
    }
    if (m_Opening.phase == OPENING_WAIT_MAIN
        || m_Opening.phase == OPENING_WAIT_OPTIONS) {
        if (m_Menu.wait == 0) {
            if (m_Opening.phase == OPENING_WAIT_MAIN
                && g_Settings.music != m_Menu.music_mode) {
                m_Menu.music_mode = g_Settings.music;
                events.audio = m_Menu.music_mode == SETTINGS_MUSIC_OFF
                    ? OPENING_AUDIO_STOP
                    : OPENING_AUDIO_PLAY;
                events.track = 0;
            }
            m_Menu.wait = 15;
        }
        if (--m_Menu.wait == 0) {
            m_Opening.phase = m_Opening.phase == OPENING_WAIT_MAIN
                ? OPENING_MAIN
                : OPENING_OPTIONS;
            m_Opening.frame_rand++;
            m_Opening.count1 = 0;
            events.clear_key_buffer = true;
        }
        return events;
    }
    if (m_Opening.phase == OPENING_QUIT) {
        return events;
    }
    m_Opening.count1 = 0;
    M_Menu(raw, &events);
    if (events.action == OPENING_ACTION_QUIT) {
        m_Opening.phase = OPENING_QUIT;
        events.audio = OPENING_AUDIO_STOP;
    }
    return events;
}

void Opening_Close(void)
{
    memset(&m_Opening, 0, sizeof(m_Opening));
    memset(&m_Menu, 0, sizeof(m_Menu));
    m_Assets = nullptr;
}

void Opening_CountSwitchedVSync(void)
{
    m_Opening.count1 = 1;
}
