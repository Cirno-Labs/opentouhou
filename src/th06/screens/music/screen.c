#include "th06/screens/music/screen.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/scene.h"
#include "th06/screens/music/comments.h"
#include "th06/sound.h"
#include "th06/translation.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#define M_CALC_PRIORITY 2
#define M_DRAW_PRIORITY 0
#define M_LIST_ROWS 10
#define M_COMMENT_SPRITES (MUSIC_COMMENT_ROWS * 2)
#define M_FIRST_TITLE_SPRITE 0x101
#define M_FIRST_COMMENT_SPRITE 0x708
#define M_TITLE_PARTS 0x100
#define M_COMMENT_PARTS 0x200

#define M_UP 0x10
#define M_DOWN 0x20
#define M_CONFIRM 0x1001
#define M_CANCEL 0xa

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
    ANM_VM comment[M_COMMENT_SPRITES];
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
} M_SCREEN;

static M_SCREEN m_Screen;

static bool M_Pressed(const uint16_t mask)
{
    return Buttons_WasPressed(&g_Buttons, mask);
}

// Shows the comment of the tune that plays, two sprites to a row, the
// second only when its half of the row has text.
static void M_ShowComment(M_SCREEN *const screen)
{
    const MUSIC_TUNE *const tune = &screen->tunes[screen->playing];
    for (int32_t i = 0; i < M_COMMENT_SPRITES; i++) {
        ANM_VM *const vm = &screen->comment[i];
        const char *half = tune->comment[i / 2][i % 2];
        // A translated row shows whole on the sprite of its first half.
        const char *const translated =
            Translation_GetMusicComment(screen->playing, i / 2);
        if (translated != nullptr) {
            half = i % 2 == 0 ? translated : "";
        }
        if (half[0] != '\0') {
            vm->visible = true;
            Result_Log(
                Anm_RenderText(vm, 0xffe0c0, 0x300000, "%s", half),
                LOG_LEVEL_WARNING);
        } else {
            vm->visible = false;
        }
        vm->pos = (VEC3) {
            (float)(i % 2) * 248.0f + 96.0f,
            320.0f + (float)(i / 2 * 16),
            0.0f,
        };
        vm->anchor = 3;
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
        } else if (screen->list_top > screen->cursor) {
            screen->list_top = screen->cursor;
        }
    }
    if (M_Pressed(M_DOWN)) {
        screen->cursor++;
        if (screen->cursor >= screen->tune_count) {
            screen->cursor = 0;
            screen->list_top = 0;
        } else if (screen->list_top <= screen->cursor - M_LIST_ROWS) {
            screen->list_top = screen->cursor - (M_LIST_ROWS - 1);
        }
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
        // The list takes input from the calc after the change, as the
        // original's intro handler reports no change.
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
        // The original reads past its titles for a list shorter than ten
        // rows.
        if (i < 0 || i >= MUSIC_TUNE_COUNT) {
            continue;
        }
        ANM_VM *const title = &screen->titles[i];
        const uint32_t color = screen->cursor != i ? 0xe0808080 : 0xffffffff;
        title->color = color;
        AsciiText_SetColor(color);
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
    for (int32_t i = 0; i < M_COMMENT_SPRITES; i++) {
        M_DrawSprite(&screen->comment[i], M_COMMENT_PARTS + (uint32_t)i);
    }
    AsciiText_SetColor(0xffffffff);
    return CHAIN_CONTINUE;
}

static void M_InitSprite(ANM_VM *const vm, const int32_t sprite)
{
    Anm_CreateVm(vm);
    Anm_InitVm(vm);
    Anm_SetSprite(vm, sprite);
}

static RESULT M_Added(void *const arg)
{
    M_SCREEN *const screen = arg;
    MUST(Anm_LoadSurface(0, "data/result/music.jpg"));
    MUST(Anm_Load(0x29, "data/music00.anm", 0x100));
    MUST(Anm_Load(0x2a, "data/music01.anm", 0x101));
    MUST(Anm_Load(0x2b, "data/music02.anm", 0x111));
    Anm_CreateVm(&screen->background);
    Anm_StartScript(&screen->background, 0x100);
    screen->state_frames = 0;
    READER reader;
    MUST(Files_Load("data/musiccmt.txt", &reader));
    screen->tunes = Mem_AllocArrZ(MUSIC_TUNE_COUNT, sizeof(MUSIC_TUNE));
    const RESULT result =
        MusicScreen_ReadComments(&reader, screen->tunes, &screen->tune_count);
    Reader_Close(&reader);
    MUST(result, "data/musiccmt.txt");
    for (int32_t i = 0; i < screen->tune_count; i++) {
        ANM_VM *const title = &screen->titles[i];
        M_InitSprite(title, M_FIRST_TITLE_SPRITE + i);
        Result_Log(
            Anm_RenderText(
                title, 0xc0e0ff, 0x302080, "%s",
                Translation_GetMusicTitle(i, screen->tunes[i].title)),
            LOG_LEVEL_WARNING);
        title->pos =
            (VEC3) { 93.0f, 104.0f + (float)((i + 1) * 18) - 20.0f, 0.0f };
        title->anchor = 3;
    }
    for (int32_t i = 0; i < M_COMMENT_SPRITES; i++) {
        M_InitSprite(&screen->comment[i], M_FIRST_COMMENT_SPRITE + i);
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
    Anm_Release(0x29);
    Anm_Release(0x2a);
    Anm_Release(0x2b);
    Chain_Remove(&screen->draw);
}

RESULT MusicScreen_Register(void)
{
    M_SCREEN *const screen = &m_Screen;
    *screen = (M_SCREEN) {};
    Chain_InitElem(&screen->calc, M_Update, screen);
    screen->calc.added = M_Added;
    screen->calc.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &screen->calc, M_CALC_PRIORITY));
    Chain_InitElem(&screen->draw, M_Draw, screen);
    return Chain_AddDraw(&g_Chain, &screen->draw, M_DRAW_PRIORITY);
}
