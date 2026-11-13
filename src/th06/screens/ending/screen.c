#include "th06/screens/ending/screen.h"

#include "th06/screens/ending/script.h"

#include "th06/anm.h"
#include "th06/files.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/scene.h"
#include "th06/sound.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

#define M_CALC_PRIORITY 3
#define M_DRAW_PRIORITY 1
#define M_SPRITE_COUNT 0x10
#define M_FIRST_TEXT_SCRIPT 0x708
#define M_FIRST_STAFF 0x600
#define M_TEXT_SHADOW 0xc0d0d0
#define M_CLEAR 99
#define M_EXTRA_RUNS 4
#define M_PATH_SIZE 0x100

#define M_CONFIRM 0x1001
#define M_SKIP 0x100

typedef enum {
    M_FADE_NONE = 0,
    M_FADE_IN_BLACK = 1,
    M_FADE_OUT_BLACK = 2,
    M_FADE_IN_WHITE = 3,
    M_FADE_OUT_WHITE = 4,
} M_FADE;

typedef enum {
    M_STEP_WAIT,
    M_STEP_END,
} M_STEP;

typedef struct {
    float scroll_x;
    float scroll_y;
    float scroll_speed;
    ANM_VM sprites[M_SPRITE_COUNT];
    ENDING_SCRIPT script;
    bool can_skip;
    TIMER timer;
    TIMER line_timer;
    TIMER wait_timer;
    int32_t wait_min_frames;
    int32_t line_min_frames;
    int32_t line_delay;
    int32_t line_delay_fast;
    int32_t line;
    uint32_t text_color;
    uint32_t fade_color;
    int32_t fade_timer;
    int32_t fade_duration;
    M_FADE fade_type;
    size_t cursor;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
} M_ENDING;

static M_ENDING m_Ending;

static bool M_IsBreak(const uint8_t c)
{
    return c == '\n' || c == '\r';
}

static uint8_t M_ByteAt(const M_ENDING *const ending, const size_t offset)
{
    return EndingScript_Byte(&ending->script, offset);
}

static uint8_t M_Current(const M_ENDING *const ending)
{
    return M_ByteAt(ending, ending->cursor);
}

static RESULT M_Load(M_ENDING *const ending, const char *const path)
{
    ENDING_SCRIPT script;
    MUST(EndingScript_Load(path, &script));
    EndingScript_Free(&ending->script);
    ending->script = script;
    ending->cursor = 0;
    ending->line_delay = 8;
    Timer_Set(&ending->line_timer, 0);
    Timer_Set(&ending->timer, 0);
    return OK;
}

// Reads a number as atoi does, then moves past it and the NULs after it.
static int32_t M_ReadInt(M_ENDING *const ending)
{
    size_t at = ending->cursor;
    while (M_ByteAt(ending, at) == ' '
           || (M_ByteAt(ending, at) >= '\t' && M_ByteAt(ending, at) <= '\r')) {
        at++;
    }
    bool negative = false;
    if (M_ByteAt(ending, at) == '-' || M_ByteAt(ending, at) == '+') {
        negative = M_ByteAt(ending, at) == '-';
        at++;
    }
    int64_t value = 0;
    while (M_ByteAt(ending, at) >= '0' && M_ByteAt(ending, at) <= '9'
           && value < INT32_MAX) {
        value = value * 10 + (M_ByteAt(ending, at) - '0');
        at++;
    }
    while (ending->cursor < ending->script.size && M_Current(ending) != '\0') {
        ending->cursor++;
    }
    while (ending->cursor < ending->script.size && M_Current(ending) == '\0') {
        ending->cursor++;
    }
    return (int32_t)(negative ? -value : value);
}

// Copies the NUL-ended path at offset into out.
static void M_ReadPath(
    const M_ENDING *const ending, const size_t offset, char out[M_PATH_SIZE])
{
    size_t length = 0;
    while (length < M_PATH_SIZE - 1 && M_ByteAt(ending, offset + length) != 0) {
        out[length] = (char)M_ByteAt(ending, offset + length);
        length++;
    }
    out[length] = '\0';
}

static void M_SkipLine(M_ENDING *const ending)
{
    while (ending->cursor < ending->script.size
           && !M_IsBreak(M_Current(ending))) {
        ending->cursor++;
    }
    while (ending->cursor < ending->script.size
           && M_IsBreak(M_Current(ending))) {
        ending->cursor++;
    }
}

static bool M_SkipPressed(const M_ENDING *const ending)
{
    return Buttons_WasPressed(&g_Buttons, M_CONFIRM)
        || (ending->can_skip && (g_Buttons.word & M_SKIP) != 0);
}

// Shows the half of start, whose bytes are text, on the sprite of the line.
static void M_ShowHalf(
    M_ENDING *const ending, const int32_t half, const size_t start,
    const uint8_t *const text, const int32_t length)
{
    const int32_t index = half + ending->line * 2;
    if (index < 0 || index >= M_SPRITE_COUNT) {
        return;
    }
    ANM_VM *const vm = &ending->sprites[index];
    Anm_StartScript(vm, M_FIRST_TEXT_SCRIPT + index);
    // A half that a command interrupts collects bytes from two places, so
    // it has no half converted at load and converts here.
    const char *const loaded = ending->cursor - start == (size_t)length
        ? EndingScript_GetHalf(&ending->script, start)
        : nullptr;
    char *converted = nullptr;
    if (loaded == nullptr) {
        size_t size = 0;
        while (size < (size_t)length && text[size] != 0) {
            size++;
        }
        if (!Result_Ignore(
                Cp932_Decode((const char *)text, size, &converted))) {
            converted = Mem_AllocZ(1);
        }
    }
    Result_Log(
        Anm_RenderText(
            vm, ending->text_color, M_TEXT_SHADOW, "%s",
            loaded != nullptr ? loaded : converted),
        LOG_LEVEL_WARNING);
    Mem_Free(converted);
}

// Shows the whole text line at the cursor on the first sprite of the line,
// and moves the cursor to the NUL or line break that ends it.
static void M_ShowLine(M_ENDING *const ending)
{
    const size_t start = ending->cursor;
    while (ending->cursor < ending->script.size && M_Current(ending) != '\0'
           && !M_IsBreak(M_Current(ending))) {
        ending->cursor++;
    }
    const int32_t index = ending->line * 2;
    if (index < 0 || index >= M_SPRITE_COUNT) {
        return;
    }
    ANM_VM *const vm = &ending->sprites[index];
    Anm_StartScript(vm, M_FIRST_TEXT_SCRIPT + index);
    const char *const line = EndingScript_GetHalf(&ending->script, start);
    Result_Log(
        Anm_RenderText(
            vm, ending->text_color, M_TEXT_SHADOW, "%s",
            line != nullptr ? line : ""),
        LOG_LEVEL_WARNING);
}

static void M_ClearScripts(M_ENDING *const ending)
{
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        ending->sprites[i].script_index = 0;
    }
}

static bool M_AnyClear(void)
{
    for (int32_t shot = 0; shot < 4; shot++) {
        for (int32_t difficulty = 0; difficulty < 4; difficulty++) {
            const CLEAR_RECORD *const clear =
                &g_GameManager.records.clears[shot];
            if (clear->without_continues[difficulty] == M_CLEAR
                || clear->with_continues[difficulty] == M_CLEAR) {
                return true;
            }
        }
    }
    return false;
}

static void M_StartFade(M_ENDING *const ending, const M_FADE type)
{
    ending->cursor++;
    ending->fade_type = type;
    ending->fade_timer = 0;
    ending->fade_duration = M_ReadInt(ending);
}

// Runs the command after an '@'. Returns false when the script ends, and
// sets *wait when the script waits.
static bool M_RunCommand(
    M_ENDING *const ending, bool *const wait, int32_t *const length,
    int32_t *const half)
{
    char path[M_PATH_SIZE];
    ending->cursor++;
    switch (M_Current(ending)) {
    case 'b':
        M_ReadPath(ending, ending->cursor + 1, path);
        if (!Result_Log(Anm_LoadSurface(0, path), LOG_LEVEL_ERROR)) {
            return false;
        }
        break;
    case 'a': {
        ending->cursor++;
        const int32_t index = M_ReadInt(ending);
        const int32_t script = M_ReadInt(ending);
        const int32_t sprite = M_ReadInt(ending);
        if (index >= 0 && index < M_SPRITE_COUNT) {
            Anm_StartScriptAtOrigin(
                &ending->sprites[index], script + M_FIRST_STAFF);
            Anm_SetSprite(&ending->sprites[index], sprite + M_FIRST_STAFF);
        }
        break;
    }
    case 'V': {
        ending->cursor++;
        const int32_t pixels = M_ReadInt(ending);
        const int32_t frames = M_ReadInt(ending);
        ending->scroll_speed = (float)pixels / (float)frames;
        break;
    }
    case 'v':
        ending->cursor++;
        ending->scroll_y = (float)M_ReadInt(ending);
        break;
    case 'F':
        M_ReadPath(ending, ending->cursor + 1, path);
        if (!Result_Log(M_Load(ending, path), LOG_LEVEL_ERROR)) {
            return false;
        }
        *length = 0;
        *half = 0;
        if (M_AnyClear()) {
            ending->can_skip = true;
        }
        // The original goes on into @R, and then skips the first line of
        // the new script.
        M_ClearScripts(ending);
        break;
    case 'R':
        M_ClearScripts(ending);
        break;
    case 'm':
        M_ReadPath(ending, ending->cursor + 1, path);
        Sound_PlayMusic(path);
        break;
    case 'M':
        ending->cursor++;
        Sound_FadeOutMusic((float)M_ReadInt(ending));
        break;
    case 's':
        ending->cursor++;
        ending->line_delay = M_ReadInt(ending);
        ending->line_delay_fast = M_ReadInt(ending);
        break;
    case 'c':
        ending->cursor++;
        ending->text_color = (uint32_t)M_ReadInt(ending);
        break;
    case 'r':
        ending->cursor++;
        Timer_Set(&ending->wait_timer, M_ReadInt(ending));
        ending->wait_min_frames = M_ReadInt(ending);
        M_SkipLine(ending);
        *wait = true;
        return true;
    case 'w':
        ending->cursor++;
        Timer_Set(&ending->line_timer, M_ReadInt(ending));
        ending->line_min_frames = M_ReadInt(ending);
        M_SkipLine(ending);
        *wait = true;
        return true;
    case '0':
        M_StartFade(ending, M_FADE_IN_BLACK);
        break;
    case '1':
        M_StartFade(ending, M_FADE_OUT_BLACK);
        break;
    case '2':
        M_StartFade(ending, M_FADE_IN_WHITE);
        break;
    case '3':
        M_StartFade(ending, M_FADE_OUT_WHITE);
        break;
    case 'z':
        return false;
    default:
        break;
    }
    M_SkipLine(ending);
    return true;
}

// Ends the text line: shows its last half and starts the wait after it.
static void M_EndLine(
    M_ENDING *const ending, const int32_t half, const size_t start,
    const uint8_t *const text, const int32_t length)
{
    if (length != 0) {
        M_ShowHalf(ending, half, start, text, length);
    }
    while (ending->cursor < ending->script.size
           && (M_Current(ending) == '\n' || M_Current(ending) == '\0'
               || M_Current(ending) == '\r')) {
        ending->cursor++;
    }
    const int32_t delay = (g_Buttons.word & M_CONFIRM) != 0
        ? ending->line_delay_fast
        : ending->line_delay;
    Timer_Set(&ending->line_timer, delay);
    ending->line_min_frames = delay;
    ending->line++;
}

// Runs the script until it waits, as the original's RunScript does.
static M_STEP M_RunScript(M_ENDING *const ending)
{
    if (ending->wait_timer.current > 0) {
        ending->wait_timer.current -= 1;
        if (ending->wait_min_frames != 0) {
            ending->wait_min_frames--;
        } else if (M_SkipPressed(ending)) {
            Timer_Set(&ending->wait_timer, 0);
        }
        if (ending->wait_timer.current > 0) {
            goto done;
        }
        for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
            ending->sprites[i] = (ANM_VM) {};
        }
        ending->line = 0;
    }
    if (ending->line_timer.current > 0) {
        ending->line_timer.current -= 1;
        if (ending->line_min_frames != 0) {
            ending->line_min_frames--;
        } else if (M_SkipPressed(ending)) {
            Timer_Set(&ending->line_timer, 0);
        }
        goto done;
    }
    {
        int32_t half = 0;
        int32_t length = 0;
        size_t start = 0;
        uint8_t text[ENDING_HALF_BYTES + 6] = {};
        for (;;) {
            if (ending->cursor >= ending->script.size) {
                return M_STEP_END;
            }
            const uint8_t c = M_Current(ending);
            if (c == '@') {
                bool wait = false;
                if (!M_RunCommand(ending, &wait, &length, &half)) {
                    return M_STEP_END;
                }
                if (wait) {
                    goto done;
                }
                continue;
            }
            if (c == '\0' || M_IsBreak(c)) {
                M_EndLine(ending, half, start, text, length);
                goto done;
            }
            if (ending->script.whole_lines) {
                M_ShowLine(ending);
                M_EndLine(ending, 0, 0, text, 0);
                goto done;
            }
            if (length == 0) {
                start = ending->cursor;
            }
            text[length] = c;
            text[length + 1] = M_ByteAt(ending, ending->cursor + 1);
            length += 2;
            ending->cursor += 2;
            if (length >= ENDING_HALF_BYTES) {
                M_ShowHalf(ending, half, start, text, length);
                if (half != 0) {
                    goto done;
                }
                half = 1;
                length = 0;
                memset(text, 0, sizeof(text));
            }
        }
    }
done:
    Timer_Tick(&ending->timer);
    ending->scroll_y -= ending->scroll_speed;
    if (ending->scroll_y <= 0.0f) {
        ending->scroll_y = 0.0f;
        ending->scroll_speed = 0.0f;
    }
    return M_STEP_WAIT;
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_ENDING *const ending = arg;
    for (int32_t run = 0;; run++) {
        if (M_RunScript(ending) == M_STEP_END) {
            return CHAIN_REMOVE;
        }
        for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
            if (ending->sprites[i].script_index != 0) {
                Anm_Step(&ending->sprites[i]);
            }
        }
        if (!ending->can_skip || (g_Buttons.word & M_SKIP) == 0
            || run >= M_EXTRA_RUNS) {
            return CHAIN_CONTINUE;
        }
    }
}

// Steps the fade and returns its colour. Fades in clear themselves when
// they end; fades out keep their last colour.
static uint32_t M_StepFade(M_ENDING *const ending)
{
    const bool done = ending->fade_timer >= ending->fade_duration;
    int32_t alpha = 0;
    if (!done && ending->fade_type != M_FADE_NONE) {
        alpha = ending->fade_timer * 0xff / ending->fade_duration;
    }
    switch (ending->fade_type) {
    case M_FADE_IN_BLACK:
    case M_FADE_IN_WHITE: {
        const uint32_t rgb =
            ending->fade_type == M_FADE_IN_WHITE ? 0xffffff : 0;
        if (done) {
            ending->fade_type = M_FADE_NONE;
            ending->fade_color = 0;
            break;
        }
        ending->fade_color = (uint32_t)(0xff - alpha) << 24 | rgb;
        ending->fade_timer++;
        break;
    }
    case M_FADE_OUT_BLACK:
    case M_FADE_OUT_WHITE: {
        const uint32_t rgb =
            ending->fade_type == M_FADE_OUT_WHITE ? 0xffffff : 0;
        if (done) {
            ending->fade_color = 0xff000000 | rgb;
            break;
        }
        ending->fade_color = (uint32_t)alpha << 24 | rgb;
        ending->fade_timer++;
        break;
    }
    case M_FADE_NONE:
        ending->fade_color = 0;
        break;
    }
    return ending->fade_color;
}

static CHAIN_STATUS M_Draw(void *const arg)
{
    M_ENDING *const ending = arg;
    Anm_CopySurfaceRectToBackBuffer(
        0, 0, 0,
        (GFX_RECT) { (int32_t)ending->scroll_x, (int32_t)ending->scroll_y, 640,
                     480 });
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        const ANM_VM *const vm = &ending->sprites[i];
        if (vm->script_index != 0) {
            ANM_DRAW draw = Anm_ScreenDraw();
            draw.view = Anm_ScreenView(
                (VIEWPORT) { 0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f },
                ANM_VIEW_SCREEN);
            draw.interpolate = true;
            draw.part = (uint32_t)i;
            Anm_DrawUnrotated(vm, &draw);
        }
    }
    const uint32_t color = M_StepFade(ending);
    if ((color & 0xff000000) != 0) {
        Anm_DrawRect((GFX_RECT) { 0, 0, 640, 480 }, color);
    }
    return CHAIN_CONTINUE;
}

static const char *M_ScriptPath(void)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    if (gm->run.difficulty == 0 || gm->stats.continues_used != 0) {
        return gm->run.character == 0 ? "data/end00b.end" : "data/end10b.end";
    }
    if (gm->run.character == 0) {
        return gm->run.shot_type == 0 ? "data/end00.end" : "data/end01.end";
    }
    return gm->run.shot_type == 0 ? "data/end10.end" : "data/end11.end";
}

static RESULT M_Added(void *const arg)
{
    M_ENDING *const ending = arg;
    GAME_MANAGER *const gm = &g_GameManager;
    gm->mode.game_cleared = true;
    MUST(Anm_Load(0x2c, "data/staff01.anm", 0x600));
    MUST(Anm_Load(0x2d, "data/staff02.anm", 0x607));
    MUST(Anm_Load(0x2e, "data/staff03.anm", 0x60e));
    CLEAR_RECORD *const clear =
        &gm->records.clears[gm->run.character * 2 + gm->run.shot_type];
    const int32_t difficulty = gm->run.difficulty;
    ending->can_skip = false;
    if (gm->stats.continues_used == 0) {
        if (clear->without_continues[difficulty] == M_CLEAR) {
            ending->can_skip = true;
        }
        clear->without_continues[difficulty] = M_CLEAR;
    } else if (clear->with_continues[difficulty] == M_CLEAR) {
        ending->can_skip = true;
    }
    clear->with_continues[difficulty] = M_CLEAR;
    return M_Load(ending, M_ScriptPath());
}

static void M_Deleted(void *const arg)
{
    M_ENDING *const ending = arg;
    Anm_Release(0x2c);
    Anm_Release(0x2d);
    Anm_Release(0x2e);
    Scene_Request(SCENE_GAME_RESULT);
    Anm_ReleaseSurface(0);
    EndingScript_Free(&ending->script);
    Chain_Remove(&ending->draw);
}

RESULT Ending_Register(void)
{
    M_ENDING *const ending = &m_Ending;
    EndingScript_Free(&ending->script);
    *ending = (M_ENDING) { .line_delay = 8 };
    Timer_Set(&ending->line_timer, 0);
    Timer_Set(&ending->timer, 0);
    Chain_InitElem(&ending->calc, M_Update, ending);
    ending->calc.added = M_Added;
    ending->calc.deleted = M_Deleted;
    MUST(Chain_AddCalc(&g_Chain, &ending->calc, M_CALC_PRIORITY));
    Chain_InitElem(&ending->draw, M_Draw, ending);
    return Chain_AddDraw(&g_Chain, &ending->draw, M_DRAW_PRIORITY);
}
