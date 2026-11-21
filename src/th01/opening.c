#include "th01/opening.h"

#include "th01/math.h"

#include "core/base/assert.h"

#include <stdio.h>
#include <string.h>

#include "th01/opening_strings.h"

static const char *const m_TrackFiles[] = {
    "reimu.mdt", "ZIPANGU.mdt", "st0.mdt",    "st1.mdt",      "st2.mdt",
    "st3.mdt",   "st4.mdt",     "st5.mdt",    "positive.mdt", "legend.mdt",
    "kami.mdt",  "tensi.mdt",   "syugen.mdt", "alice.mdt",    "iris.mdt",
};

static void M_Image(
    VIDEO *const video, const PI_IMAGE *const image, const int32_t key)
{
    const VIDEO_IMAGE view = {
        .pixels = image->pixels,
        .width = image->width,
        .height = image->height,
        .stride = image->width,
        .key = key,
    };
    Video_Blit(
        video, &view, (GFX_RECT) { 0, 0, image->width, image->height }, 0, 0);
}

static void M_Text(
    OPENING *const opening, const char *const text, const int32_t left,
    const int32_t top, const uint8_t color, const GAME_TEXT_WEIGHT weight)
{
    const GAME_TEXT_STYLE style = { .weight = weight };
    const bool drawn = Result_Ignore(GameText_Draw(
        &opening->assets->font, opening->video, text, left, top, color,
        &style));
    ASSERT(drawn);
}

static int32_t M_Top(const int32_t count)
{
    return 316 - count * 10;
}

static void M_Erase(OPENING *const opening, const int32_t count)
{
    Video_Restore16(
        opening->video, (GFX_RECT) { 220, M_Top(count), 176, count * 20 });
}

static void M_Choice(
    OPENING *const opening, const int32_t choice, const uint8_t color)
{
    if (opening->phase == OPENING_MAIN) {
        M_Text(
            opening, m_MainMenuItems[choice], 244, M_Top(4) + choice * 20,
            color, GAME_TEXT_BLACK);
        return;
    }
    const bool music = opening->phase == OPENING_MUSIC;
    const int32_t top = music ? M_Top(3) + choice * 40 : M_Top(5) + choice * 20;
    Video_Restore16(opening->video, (GFX_RECT) { 228, top, 176, 16 });
    char text[256];
    if (music) {
        if (choice == 0) {
            Video_Restore16(
                opening->video, (GFX_RECT) { 228, top + 20, 192, 16 });
            snprintf(
                text, sizeof(text), "%s%.2d", m_MusicMenuItems[0],
                opening->track);
            M_Text(opening, text, 228, top, color, GAME_TEXT_BLACK);
            M_Text(
                opening, m_TrackTitles[opening->track], 228, top + 20, color,
                GAME_TEXT_BLACK);
        } else {
            M_Text(
                opening, m_MusicMenuItems[1], 228, top, color, GAME_TEXT_BLACK);
        }
        return;
    }
    const char *value = "";
    if (choice == 0) {
        value = m_Ranks[opening->settings->difficulty];
    } else if (choice == 1) {
        value = m_MusicModes[opening->settings->music];
    } else if (choice == 2) {
        value = m_Lives[opening->settings->lives_extra];
    }
    snprintf(text, sizeof(text), "%s%s", m_OptionMenuItems[choice], value);
    M_Text(opening, text, 228, top, color, GAME_TEXT_BLACK);
}

static void M_Lines(OPENING *const opening)
{
    for (int32_t i = 0; i < 2; i++) {
        Video_Restore16(
            opening->video,
            (GFX_RECT) { 0, opening->line_y[i], VIDEO_WIDTH, 1 });
    }
    for (int32_t i = 0; i < 2; i++) {
        int32_t y;
        do {
            y = Math_Random(&opening->random) % VIDEO_HEIGHT;
        } while (opening->drawn_rows[y]);
        opening->drawn_rows[y] = true;
        opening->line_y[i] = y;
        Video_HorizontalLine(opening->video, 0, VIDEO_WIDTH - 1, y, 15);
    }
    opening->line_count++;
}

static void M_Window(OPENING *const opening)
{
    VIDEO *const video = opening->video;
    Video_Copy(
        video, 1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    M_Image(video, &opening->assets->window, 15);
    Palette_Import(&opening->palette.resource, opening->assets->window.palette);
    Video_Copy(
        video, 0, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 1, 0, 0);
    opening->random = opening->frame_rand;
    opening->phase = OPENING_MAIN;
    opening->entering = true;
}

static int32_t M_Ring(
    const int32_t value, const int32_t delta, const int32_t count)
{
    return (value + delta + count) % count;
}

static void M_Horizontal(OPENING *const opening, const int32_t delta)
{
    if (opening->phase == OPENING_MUSIC) {
        if (opening->selection == 0) {
            opening->track = M_Ring(opening->track, delta, OPENING_TRACK_COUNT);
        }
    } else if (opening->selection == 0) {
        opening->settings->difficulty =
            M_Ring(opening->settings->difficulty, delta, 4);
    } else if (opening->selection == 1) {
        opening->settings->music = M_Ring(opening->settings->music, delta, 2);
    } else if (opening->selection == 2) {
        opening->settings->lives_extra =
            M_Ring(opening->settings->lives_extra, delta, 5);
    }
    M_Choice(opening, opening->selection, 15);
}

static void M_Menu(
    OPENING *const opening, const TH01_INPUT *const raw,
    OPENING_EVENTS *const events)
{
    const OPENING_PHASE phase = opening->phase;
    const bool main = phase == OPENING_MAIN;
    const bool music = phase == OPENING_MUSIC;
    const int32_t count = main ? 4 : (music ? 2 : 5);
    const int32_t previous = opening->selection;
    // Discard entry selection after sensing, including the stale music exit 3.
    opening->selection = GameInput_SenseMenu(
        &opening->input, raw, main ? MENU_INPUT_MAIN : MENU_INPUT_OPTIONS,
        main ? 4 : opening->option_item_count,
        opening->entering && !main ? 0 : opening->selection);
    if (opening->entering) {
        opening->entering = false;
        if (!main) {
            opening->selection = 0;
            opening->option_item_count = count;
        }
        if (music) {
            opening->input.ok = false;
            opening->input.shot = false;
        }
        M_Erase(opening, phase == OPENING_OPTIONS ? 4 : 5);
        for (int32_t i = 0; i < count; i++) {
            M_Choice(opening, i, 5);
        }
        M_Choice(opening, opening->selection, 15);
    } else if (previous != opening->selection) {
        M_Choice(opening, previous, 5);
        M_Choice(opening, opening->selection, 15);
    }
    if (!main) {
        const int32_t slot = music ? 1 : 0;
        if (opening->input.left && !opening->left_locked[slot]) {
            M_Horizontal(opening, -1);
        }
        opening->left_locked[slot] = opening->input.left;
        if (opening->input.right && !opening->right_locked[slot]) {
            M_Horizontal(opening, 1);
        }
        opening->right_locked[slot] = opening->input.right;
    }
    const bool confirm = opening->input.ok || opening->input.shot;
    if (main) {
        if (confirm) {
            if (opening->selection < 2) {
                opening->exit_action = opening->selection == 0
                    ? OPENING_ACTION_START
                    : OPENING_ACTION_CONTINUE;
                opening->phase = OPENING_EXIT_FADE;
                Palette_BeginFade(
                    &opening->fade, &opening->palette, PALETTE_FADE_BLACK_OUT);
                events->audio = OPENING_AUDIO_FADE;
                return;
            } else if (opening->selection == 2) {
                opening->phase = OPENING_OPTIONS;
                opening->entering = true;
            } else {
                events->action = OPENING_ACTION_QUIT;
            }
        }
        if (opening->input.cancel) {
            events->action = OPENING_ACTION_QUIT;
        }
    } else if (!music) {
        if ((confirm && opening->selection == 4) || opening->input.cancel) {
            opening->phase = OPENING_WAIT_MAIN;
            opening->entering = true;
            opening->selection = 2;
        }
        if (confirm && opening->selection == 3) {
            opening->phase = OPENING_MUSIC;
            opening->entering = true;
            opening->selection = 0;
        }
    } else {
        if ((confirm && opening->selection == 1) || opening->input.cancel) {
            opening->phase = OPENING_WAIT_OPTIONS;
            opening->entering = true;
            opening->selection = 3;
        }
        if (confirm && opening->selection == 0) {
            events->audio = OPENING_AUDIO_PLAY;
            events->track = opening->track;
            opening->track_starts++;
        }
    }
    events->clear_key_buffer = true;
    opening->frame_rand++;
}

const char *Opening_TrackFile(const int32_t track)
{
    ASSERT(track >= 0 && track < OPENING_TRACK_COUNT);
    return m_TrackFiles[track];
}

OPENING_EVENTS Opening_Init(
    OPENING *const opening, VIDEO *const video, TH01_SETTINGS *const settings,
    const OPENING_ASSETS *const assets)
{
    *opening = (OPENING) {
        .video = video,
        .settings = settings,
        .assets = assets,
        .phase = OPENING_FADE,
        .random = 1,
        .option_item_count = 4,
        .music_mode = settings->music,
    };
    Palette_Init(&opening->palette);
    Video_SelectDrawingPage(video, 1);
    Video_SelectVisiblePage(video, 0);
    M_Image(video, &assets->title, -1);
    Palette_Import(&opening->palette.resource, assets->title.palette);
    Palette_Set(&opening->palette, video, &opening->palette.resource);
    Palette_ShowSolid(video, 0, 0, 0);
    Video_Copy(
        video, 1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    M_Image(video, &assets->revealed, -1);
    Palette_Import(&opening->palette.resource, assets->revealed.palette);
    Video_SelectDrawingPage(video, 0);
    Palette_BeginFade(&opening->fade, &opening->palette, PALETTE_FADE_BLACK_IN);
    return (OPENING_EVENTS) { .audio = OPENING_AUDIO_PLAY,
                              .track = 0,
                              .clear_key_buffer = true };
}

OPENING_EVENTS Opening_Tick(
    OPENING *const opening, const TH01_INPUT *const raw, const bool any_key)
{
    OPENING_EVENTS events = {};
    if (opening->phase == OPENING_EXIT_FADE) {
        Palette_FadeStep(&opening->fade, &opening->palette, opening->video);
        if (opening->fade.remaining == 0) {
            events.action = opening->exit_action;
        }
        return events;
    }
    opening->count1++;
    if (opening->phase == OPENING_FADE) {
        const bool stepped =
            Palette_FadeStep(&opening->fade, &opening->palette, opening->video);
        ASSERT(stepped);
        if (opening->fade.remaining == 0) {
            opening->phase = OPENING_HOLD;
        }
        return events;
    }
    if (opening->phase == OPENING_HOLD) {
        if (opening->count1 < 100) {
            return events;
        }
        opening->count1 = 0;
        opening->phase = OPENING_LINES;
        M_Lines(opening);
        return events;
    }
    if (opening->phase == OPENING_LINES) {
        opening->count1 = 0;
        if (opening->line_count < 100) {
            M_Lines(opening);
            return events;
        }
        Video_Copy(
            opening->video, 1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT },
            0, 0, 0);
        opening->phase = OPENING_PROMPT;
    }
    if (opening->phase == OPENING_PROMPT) {
        if (!any_key) {
            if (opening->count1 != 0) {
                opening->count1 = 0;
                if (opening->prompt_frame % 70 < 50) {
                    M_Text(
                        opening, m_Prompt, 244, M_Top(1), 15, GAME_TEXT_BOLD);
                } else {
                    Video_Restore16(
                        opening->video, (GFX_RECT) { 244, M_Top(1), 120, 16 });
                }
                opening->prompt_frame = (opening->prompt_frame + 1) % 70;
            }
            return events;
        }
        M_Window(opening);
    }
    if (opening->phase == OPENING_WAIT_MAIN
        || opening->phase == OPENING_WAIT_OPTIONS) {
        if (opening->wait == 0) {
            if (opening->phase == OPENING_WAIT_MAIN
                && opening->settings->music != opening->music_mode) {
                opening->music_mode = opening->settings->music;
                events.audio = opening->music_mode == SETTINGS_MUSIC_OFF
                    ? OPENING_AUDIO_STOP
                    : OPENING_AUDIO_PLAY;
                events.track = 0;
            }
            opening->wait = 15;
        }
        if (--opening->wait == 0) {
            opening->phase = opening->phase == OPENING_WAIT_MAIN
                ? OPENING_MAIN
                : OPENING_OPTIONS;
            opening->frame_rand++;
            opening->count1 = 0;
            events.clear_key_buffer = true;
        }
        return events;
    }
    if (opening->phase == OPENING_QUIT) {
        return events;
    }
    opening->count1 = 0;
    M_Menu(opening, raw, &events);
    if (events.action == OPENING_ACTION_QUIT) {
        opening->phase = OPENING_QUIT;
        events.audio = OPENING_AUDIO_STOP;
    }
    return events;
}
