#include "th07/screens/music/screen.h"

#include "th07/anm.h"
#include "th07/ascii.h"
#include "th07/game.h"
#include "th07/scene.h"
#include "th07/screens/music/comments.h"
#include "th07/sound.h"

#include "stg/translation.h"

#include "shell/game.h"
#include "core/vfs.h"
#include "core/base/color.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

#define M_ANM_SLOT 0x2e
#define M_CALC_PRIORITY 3
#define M_DRAW_PRIORITY 0
#define M_LIST_ROWS 10
#define M_FIRST_SPRITE 0x900
#define M_FIRST_TITLE_SPRITE 0x901
#define M_FIRST_COMMENT_SPRITE 0x707
#define M_TITLE_PARTS 0x100
#define M_COMMENT_PARTS 0x200

#define M_UP 0x10
#define M_DOWN 0x20
#define M_CONFIRM 0x1001
#define M_CANCEL 0xa

#define M_SELECTED_INTERRUPT 1
#define M_UNSELECTED_INTERRUPT 2

typedef enum {
    M_STATE_INTRO = 0,
    M_STATE_LIST = 1,
} M_STATE;

typedef struct {
    int32_t state_frames;
    M_STATE state;
    int32_t cursor;
    int32_t playing;
    int32_t list_top;
    int32_t tune_count;
    MUSIC_TUNE *tunes;
    ANM_VM background;
    ANM_VM titles[MUSIC_TUNE_COUNT];
    ANM_VM comment[MUSIC_COMMENT_ROWS];
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
} M_SCREEN;

static M_SCREEN m_Screen;

static bool M_Pressed(const uint16_t mask)
{
    return Buttons_WasPressed(&g_Buttons, mask);
}

// Sends interrupt 1 to the title under the cursor and 2 to the others.
static void M_HighlightCursor(M_SCREEN *const screen)
{
    for (int32_t i = 0; i < MUSIC_TUNE_COUNT; i++) {
        screen->titles[i].pending_interrupt =
            i == screen->cursor ? M_SELECTED_INTERRUPT : M_UNSELECTED_INTERRUPT;
    }
}

// Shows the comment of the tune that plays, one sprite to a row.
static void M_ShowComment(M_SCREEN *const screen)
{
    const MUSIC_TUNE *const tune = &screen->tunes[screen->playing];
    for (int32_t i = 0; i < MUSIC_COMMENT_ROWS; i++) {
        ANM_VM *const vm = &screen->comment[i];
        const char *const translated =
            Translation_GetMusicComment(screen->playing, i);
        const char *const row =
            translated != nullptr ? translated : tune->comment[i];
        if (row[0] != '\0') {
            vm->visible = true;
            Result_Log(
                Anm_RenderText(vm, 0xffe0c0, 0x300000, "%s", row),
                LOG_LEVEL_WARNING);
        } else {
            vm->visible = false;
        }
        vm->pending_interrupt = M_SELECTED_INTERRUPT;
    }
}

// Returns whether the screen leaves.
static bool M_UpdateList(M_SCREEN *const screen)
{
    if (M_Pressed(M_UP)) {
        screen->cursor--;
        if (screen->cursor < 0) {
            screen->cursor = screen->tune_count - 1;
            screen->list_top = screen->tune_count - M_LIST_ROWS;
            if (screen->list_top < 0) {
                screen->list_top = 0;
            }
        } else if (screen->list_top > screen->cursor) {
            screen->list_top = screen->cursor;
        }
        M_HighlightCursor(screen);
    }
    if (M_Pressed(M_DOWN)) {
        screen->cursor++;
        if (screen->cursor >= screen->tune_count) {
            screen->cursor = 0;
            screen->list_top = 0;
        } else if (screen->list_top <= screen->cursor - M_LIST_ROWS) {
            screen->list_top = screen->cursor - (M_LIST_ROWS - 1);
        }
        M_HighlightCursor(screen);
    }
    if (M_Pressed(M_CONFIRM) && screen->tune_count != 0) {
        screen->playing = screen->cursor;
        Sound_PlayMusic(screen->tunes[screen->playing].path);
        M_ShowComment(screen);
    }
    if (M_Pressed(M_CANCEL)) {
        Scene_Request(SCENE_TITLE);
        return true;
    }
    return false;
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_SCREEN *const screen = arg;
    const M_STATE old_state = screen->state;
    switch (screen->state) {
    case M_STATE_INTRO:
        if (screen->state_frames == 0) {
            M_HighlightCursor(screen);
            for (int32_t i = 0; i < MUSIC_COMMENT_ROWS; i++) {
                screen->comment[i].pending_interrupt = M_SELECTED_INTERRUPT;
            }
        }
        if (screen->state_frames >= 8) {
            screen->state = M_STATE_LIST;
        }
        break;
    case M_STATE_LIST:
        if (M_UpdateList(screen)) {
            return CHAIN_REMOVE;
        }
        break;
    }
    if (old_state != screen->state) {
        screen->state_frames = 0;
    } else {
        screen->state_frames++;
    }
    Anm_Step(&screen->background);
    for (int32_t i = 0; i < MUSIC_TUNE_COUNT; i++) {
        Anm_Step(&screen->titles[i]);
    }
    for (int32_t i = 0; i < MUSIC_COMMENT_ROWS; i++) {
        Anm_Step(&screen->comment[i]);
    }
    return CHAIN_CONTINUE;
}

static void M_DrawSprite(const ANM_VM *const vm, const uint32_t part)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = Anm_ScreenView(
        (VIEWPORT) { 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f }, ANM_VIEW_SCREEN);
    draw.interpolate = true;
    draw.part = part;
    Anm_DrawUnrotated(vm, &draw);
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    M_SCREEN *const screen = arg;
    Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    M_DrawSprite(&screen->background, 0);
    for (int32_t i = screen->list_top; i < screen->list_top + M_LIST_ROWS;
         i++) {
        if (i >= screen->tune_count) {
            break;
        }
        ANM_VM *const title = &screen->titles[i];
        AsciiText_SetColor(title->color);
        title->pos = (VEC3) {
            93.0f,
            104.0f + (float)((i + 1 - screen->list_top) * 18) - 20.0f,
            0.0f,
        };
        M_DrawSprite(title, M_TITLE_PARTS + (uint32_t)i);
        VEC3 pos = title->pos;
        pos.x -= 60.0f;
        if (screen->cursor == i) {
            AsciiText_AddString(pos, "\x7f");
        }
        pos.x += 15.0f;
        AsciiText_AddFormat(pos, "%2d.", i + 1);
    }
    for (int32_t i = 0; i < MUSIC_COMMENT_ROWS; i++) {
        M_DrawSprite(&screen->comment[i], M_COMMENT_PARTS + (uint32_t)i);
    }
    AsciiText_SetColor(COLOR_WHITE);
    return CHAIN_CONTINUE;
}

static RESULT M_Added(void *const arg)
{
    M_SCREEN *const screen = arg;
    MUST(Anm_LoadSurface(0, "data/result/music.jpg"));
    MUST(Anm_Load(M_ANM_SLOT, "data/music00.anm", M_FIRST_SPRITE));
    Anm_CreateVm(&screen->background);
    Anm_StartScript(&screen->background, M_FIRST_SPRITE);
    screen->state_frames = 0;
    READER reader;
    MUST(Vfs_Open(
        Shell_GetFiles(), "data/musiccmt.txt", VFS_IN_MEMORY, &reader));
    screen->tunes = Mem_AllocArrZ(MUSIC_TUNE_COUNT, sizeof(MUSIC_TUNE));
    const RESULT result =
        MusicScreen_ReadComments(&reader, screen->tunes, &screen->tune_count);
    Reader_Close(&reader);
    MUST(result, "data/musiccmt.txt");
    for (int32_t i = 0; i < screen->tune_count; i++) {
        ANM_VM *const title = &screen->titles[i];
        Anm_CreateVm(title);
        Anm_StartScript(title, M_FIRST_TITLE_SPRITE + i);
        Result_Log(
            Anm_RenderText(
                title, 0xc0e0ff, 0x302080, "%s",
                Translation_GetMusicTitle(i, screen->tunes[i].title)),
            LOG_LEVEL_WARNING);
        title->pos =
            (VEC3) { 93.0f, 104.0f + (float)((i + 1) * 18) - 20.0f, 0.0f };
        title->anchor = 3;
    }
    for (int32_t i = 0; i < MUSIC_COMMENT_ROWS; i++) {
        Anm_CreateVm(&screen->comment[i]);
        Anm_StartScript(&screen->comment[i], M_FIRST_COMMENT_SPRITE + i);
    }
    M_ShowComment(screen);
    return OK;
}

static void M_Deleted(void *const arg)
{
    M_SCREEN *const screen = arg;
    Mem_Free(screen->tunes);
    screen->tunes = nullptr;
    Anm_ReleaseSurface(0);
    Anm_Release(M_ANM_SLOT);
    Chain_Remove(&screen->draw);
}

RESULT MusicScreen_Register(void)
{
    M_SCREEN *const screen = &m_Screen;
    memset(screen, 0, sizeof(*screen));
    Chain_InitElem(&screen->calc, M_Update, screen);
    screen->calc.added = M_Added;
    screen->calc.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &screen->calc, M_CALC_PRIORITY));
    Chain_InitElem(&screen->draw, M_Draw, screen);
    return Chain_AddDraw(&g_Chain, &screen->draw, M_DRAW_PRIORITY);
}
