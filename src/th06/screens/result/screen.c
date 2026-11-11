#include "th06/screens/result/screen.h"

#include "th06/anm.h"
#include "th06/ascii.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/scene.h"
#include "th06/screens/result/private.h"
#include "th06/sound.h"
#include "th06/strings.h"
#include "th06/translation.h"

#include "core/base/assert.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

#define M_CALC_PRIORITY 13
#define M_DRAW_PRIORITY 12
#define M_SPRITE_COUNT 0x26
#define M_TEXT_SPRITE_COUNT 0x10
#define M_FIRST_SCRIPT 0x100
#define M_FIRST_TEXT_SPRITE 0x708
#define M_SLOT_SPRITE 0x16
#define M_ALPHABET_SIZE 0x60
#define M_ALPHABET_SPACE 0x5e
#define M_ALPHABET_END 0x5f
#define M_NAME_LENGTH 8
#define M_CLEAR_STAGE 99
#define M_SPELL_PAGE_SIZE 10

#define M_UP 0x10
#define M_DOWN 0x20
#define M_LEFT 0x40
#define M_RIGHT 0x80
#define M_MENU 0x8
#define M_CONFIRM 0x1001
#define M_CANCEL 0xa

typedef enum {
    M_STATE_OPEN_MENU = 0,
    M_STATE_MENU = 1,
    M_STATE_LEAVE = 2,
    M_STATE_SCORES_EASY = 3,
    M_STATE_SCORES_NORMAL = 4,
    M_STATE_SCORES_HARD = 5,
    M_STATE_SCORES_LUNATIC = 6,
    M_STATE_SCORES_EXTRA = 7,
    M_STATE_SPELL_CARDS = 8,
    M_STATE_NAME_ENTRY = 9,
    M_STATE_REPLAY_PROMPT = 10,
    M_STATE_REPLAY_REFUSED = 11,
    M_STATE_REPLAY_SLOTS = 12,
    M_STATE_REPLAY_NAME = 13,
    M_STATE_REPLAY_OVERWRITE = 14,
    M_STATE_STATISTICS = 15,
    M_STATE_STATISTICS_LEAVE = 16,
    M_STATE_PRACTICE = 17,
} M_STATE;

typedef struct {
    bool score_loaded;
    int32_t frame;
    M_STATE state;
    int32_t list_state;
    int32_t cursor;
    int32_t saved_cursor;
    int32_t saved_spell_page;
    int32_t replay_slot;
    int32_t char_index;
    int32_t character;
    int32_t spell_page;
    int32_t difficulty;
    int32_t cheat_progress;
    uint8_t name[12];
    ANM_VM sprites[M_SPRITE_COUNT];
    ANM_VM text_sprites[M_TEXT_SPRITE_COUNT];
    SCORE_LIST scores[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];
    // Points at the entry being named in its list, or at entry when it
    // ranks below the tenth.
    HIGH_SCORE *named;
    HIGH_SCORE entry;
    SAVED_REPLAY slots[SAVED_REPLAY_SLOT_COUNT];
    char replay_date[9];
    uint32_t replay_score;
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
} M_SCREEN;

// Holds the glyphs of the name entry grid, 16 to a row, as CP932 bytes.
// The last two cells stand for a space and for the end of the name.
static const uint8_t m_NameAlphabet[M_ALPHABET_SIZE + 1] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ.,:;\xa5@abcdefghijklmnopqrstuvwxyz+-/*=%"
    "0123456789(){}[]<>#!?'\"$      --";

static const STRING m_ShotNames[4] = {
    STRING_SHOT_NAME_REIMU_A,
    STRING_SHOT_NAME_REIMU_B,
    STRING_SHOT_NAME_MARISA_A,
    STRING_SHOT_NAME_MARISA_B,
};

static const STRING m_ShotTypeNames[4] = {
    STRING_SHOT_TYPE_REIMU_A,
    STRING_SHOT_TYPE_REIMU_B,
    STRING_SHOT_TYPE_MARISA_A,
    STRING_SHOT_TYPE_MARISA_B,
};

static const STRING m_DifficultyNames[5] = {
    STRING_RESULT_DIFFICULTY_EASY,  STRING_RESULT_DIFFICULTY_NORMAL,
    STRING_RESULT_DIFFICULTY_HARD,  STRING_RESULT_DIFFICULTY_LUNATIC,
    STRING_RESULT_DIFFICULTY_EXTRA,
};

static M_SCREEN m_Screen;

static bool M_Pressed(const uint16_t mask)
{
    return Buttons_WasPressed(&g_Buttons, mask);
}

static bool M_PressedOrRepeated(const uint16_t mask)
{
    return Buttons_WasPressedOrRepeated(&g_Buttons, mask);
}

static void M_Interrupt(M_SCREEN *const screen, const int16_t interrupt)
{
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        screen->sprites[i].pending_interrupt = interrupt;
    }
}

static void M_RenderShotNames(M_SCREEN *const screen)
{
    for (int32_t i = 0; i < 2; i++) {
        Result_Log(
            Anm_RenderTextCentered(
                &screen->text_sprites[i], 0xffffff, 0, "%s",
                g_Strings[m_ShotNames[screen->character * 2 + i]]),
            LOG_LEVEL_WARNING);
    }
}

static void M_MoveCursorVertical(M_SCREEN *const screen, const int32_t count)
{
    if (M_PressedOrRepeated(M_UP)) {
        screen->cursor--;
        if (screen->cursor < 0) {
            screen->cursor += count;
        }
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_DOWN)) {
        screen->cursor++;
        if (screen->cursor >= count) {
            screen->cursor -= count;
        }
        Sound_Queue(SOUND_SELECT);
    }
}

// Returns whether the cursor moved. Left takes precedence over right.
static bool M_MoveCursorHorizontal(M_SCREEN *const screen, const int32_t count)
{
    if (M_PressedOrRepeated(M_LEFT)) {
        screen->cursor--;
        if (screen->cursor < 0) {
            screen->cursor += count;
        }
        Sound_Queue(SOUND_SELECT);
        return true;
    }
    if (M_PressedOrRepeated(M_RIGHT)) {
        screen->cursor++;
        if (screen->cursor >= count) {
            screen->cursor -= count;
        }
        Sound_Queue(SOUND_SELECT);
        return true;
    }
    return false;
}

// Moves the name entry cursor over the grid, skipping the empty cells.
static void M_MoveCharIndex(M_SCREEN *const screen)
{
    if (M_PressedOrRepeated(M_UP)) {
        do {
            screen->char_index -= 16;
            if (screen->char_index < 0) {
                screen->char_index += M_ALPHABET_SIZE;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_DOWN)) {
        do {
            screen->char_index += 16;
            if (screen->char_index >= M_ALPHABET_SIZE) {
                screen->char_index -= M_ALPHABET_SIZE;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_LEFT)) {
        do {
            screen->char_index--;
            if (screen->char_index % 16 == 15) {
                screen->char_index += 16;
            }
            if (screen->char_index < 0) {
                screen->char_index = 15;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
    if (M_PressedOrRepeated(M_RIGHT)) {
        do {
            screen->char_index++;
            if (screen->char_index % 16 == 0) {
                screen->char_index -= 16;
            }
        } while (m_NameAlphabet[screen->char_index] == ' ');
        Sound_Queue(SOUND_SELECT);
    }
}

// Returns the position that the next character of a name goes to. A
// cursor at the end of the name replaces the last character.
static int32_t M_NamePosition(const M_SCREEN *const screen)
{
    return screen->cursor >= M_NAME_LENGTH ? M_NAME_LENGTH - 1 : screen->cursor;
}

static void M_AdvanceNameCursor(M_SCREEN *const screen)
{
    if (screen->cursor < M_NAME_LENGTH) {
        screen->cursor++;
        if (screen->cursor == M_NAME_LENGTH) {
            screen->char_index = M_ALPHABET_END;
        }
    }
    Sound_Queue(SOUND_OK);
}

// Moves the cursor back and writes a space where the cursor was, as the
// original does. The character before the cursor therefore stays until a
// new character replaces it, except at the end of the name, where the
// last character is cleared.
static void M_EraseNameCharacter(M_SCREEN *const screen, uint8_t *const name)
{
    const int32_t position = M_NamePosition(screen);
    if (screen->cursor > 0) {
        screen->cursor--;
        name[position] = ' ';
    }
    Sound_Queue(SOUND_CANCEL);
}

static void M_EndNameEntry(M_SCREEN *const screen)
{
    screen->state = M_STATE_STATISTICS;
    screen->frame = 0;
    M_Interrupt(screen, 2);
    memcpy(screen->name, screen->named->name, sizeof(screen->named->name));
}

static void M_StartNameEntry(M_SCREEN *const screen)
{
    screen->character = g_GameManager.run.character;
    screen->difficulty = g_GameManager.run.difficulty;
    M_Interrupt(screen, (int16_t)(screen->difficulty + 3));
    M_RenderShotNames(screen);
    if (g_GameManager.run.shot_type != 0) {
        screen->text_sprites[0].color = 0x80ffffff;
    }
    if (g_GameManager.run.shot_type != 1) {
        screen->text_sprites[1].color = 0x80ffffff;
    }
    const int32_t shot = screen->character * 2 + g_GameManager.run.shot_type;
    screen->entry = (HIGH_SCORE) {
        .magic = HIGH_SCORE_MAGIC,
        .is_new = true,
        .score = g_GameManager.score.value,
        .shot = (uint8_t)shot,
        .difficulty = (uint8_t)screen->difficulty,
        .stage = g_GameManager.mode.game_cleared
            ? M_CLEAR_STAGE
            : (uint8_t)g_GameManager.run.stage,
    };
    memcpy(screen->entry.name, "        ", sizeof(screen->entry.name));
    SCORE_LIST *const list = &screen->scores[screen->difficulty][shot];
    const int32_t rank = ScoreList_Insert(list, &screen->entry);
    if (rank >= SCORE_LIST_SIZE) {
        screen->named = &screen->entry;
        M_EndNameEntry(screen);
        return;
    }
    screen->named = &list->entries[rank];
    screen->cursor = 0;
    screen->name[0] = 0;
}

static void M_UpdateNameEntry(M_SCREEN *const screen)
{
    if (screen->frame == 0) {
        M_StartNameEntry(screen);
        if (screen->state != M_STATE_NAME_ENTRY) {
            return;
        }
    }
    if (screen->frame < 30) {
        return;
    }
    M_MoveCharIndex(screen);
    uint8_t *const name = screen->named->name;
    if (M_PressedOrRepeated(M_CONFIRM)) {
        if (screen->char_index == M_ALPHABET_END) {
            Sound_Queue(SOUND_CANCEL);
            M_EndNameEntry(screen);
            return;
        }
        name[M_NamePosition(screen)] = screen->char_index < M_ALPHABET_SPACE
            ? m_NameAlphabet[screen->char_index]
            : ' ';
        M_AdvanceNameCursor(screen);
    }
    if (M_PressedOrRepeated(M_CANCEL)) {
        M_EraseNameCharacter(screen, name);
    }
    if (M_Pressed(M_MENU)) {
        Sound_Queue(SOUND_CANCEL);
        M_EndNameEntry(screen);
    }
}

static void M_Leave(M_SCREEN *const screen)
{
    screen->frame = 0;
    Sound_Queue(SOUND_CANCEL);
    screen->state = M_STATE_LEAVE;
    M_Interrupt(screen, 2);
}

static void M_HighlightChoice(M_SCREEN *const screen)
{
    ANM_VM *const yes = &screen->sprites[0x10];
    ANM_VM *const no = &screen->sprites[0x11];
    const bool first = screen->cursor == 0;
    yes->color = (yes->color & 0xff000000) | (first ? 0xff6060 : 0x606060);
    no->color = (no->color & 0xff000000) | (first ? 0x606060 : 0xff6060);
}

static bool M_SlotIsUsed(const M_SCREEN *const screen, const int32_t slot)
{
    return screen->slots[slot].valid;
}

static void M_ListSlots(M_SCREEN *const screen)
{
    if (screen->frame == 0) {
        for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
            const SAVED_REPLAY *const slot = ResultSaves_GetSlot(i);
            if (slot->valid) {
                screen->slots[i] = *slot;
            }
        }
    }
    if (screen->frame < 20) {
        return;
    }
    M_MoveCursorVertical(screen, SAVED_REPLAY_SLOT_COUNT);
    screen->replay_slot = screen->cursor;
    if (M_Pressed(M_CONFIRM)) {
        Sound_Queue(SOUND_OK);
        screen->replay_slot = screen->cursor;
        screen->frame = 0;
        memcpy(
            screen->replay_date, ResultSaves_GetClock()->date,
            sizeof(screen->replay_date));
        screen->replay_score = g_GameManager.score.value;
        const bool used = M_SlotIsUsed(screen, screen->cursor);
        M_Interrupt(screen, used ? 0xb : 0xf);
        screen->sprites[screen->replay_slot + M_SLOT_SPRITE].pending_interrupt =
            0xe;
        screen->state = used ? M_STATE_REPLAY_OVERWRITE : M_STATE_REPLAY_NAME;
        screen->cursor = 0;
        screen->char_index = 0;
    }
    if (M_Pressed(M_CANCEL)) {
        Sound_Queue(SOUND_CANCEL);
        screen->state = M_STATE_REPLAY_PROMPT;
        M_Interrupt(screen, 2);
        screen->frame = 0;
    }
}

static void M_ChooseSlot(M_SCREEN *const screen)
{
    Sound_Queue(SOUND_OK);
    screen->state = M_STATE_REPLAY_SLOTS;
    M_Interrupt(screen, 10);
    screen->frame = 0;
    M_ListSlots(screen);
}

static void M_SaveReplay(M_SCREEN *const screen)
{
    size_t length = 0;
    while (length < sizeof(screen->name) && screen->name[length] != 0) {
        length++;
    }
    char *name;
    if (!Result_Log(
            Cp932_Decode((const char *)screen->name, length, &name),
            LOG_LEVEL_WARNING)) {
        return;
    }
    const RESULT_SCREEN_CLOCK *const clock = ResultSaves_GetClock();
    const REPLAY_SAVE_INFO info = {
        .name = name,
        .date = clock->date,
        .achieved_frames = clock->achieved_frames,
        .expected_frames = clock->expected_frames,
    };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    const bool saved =
        Result_Log(Replay_Save(&writer, &info), LOG_LEVEL_WARNING);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    if (saved && size != 0) {
        ResultSaves_PutReplay(screen->replay_slot, data, size);
    } else if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    Mem_Free(name);
}

static void M_UpdateReplayPrompt(M_SCREEN *const screen)
{
    if (screen->frame == 60) {
        // Continued games cannot be saved. The original also refuses a
        // game speed below full speed, which the port never runs at.
        const int16_t interrupt =
            g_GameManager.stats.continues_used != 0 ? 0xc : 9;
        // The original starts at the second sprite and runs one past the
        // last, onto the first text sprite, which never runs a script.
        for (int32_t i = 1; i < M_SPRITE_COUNT; i++) {
            screen->sprites[i].pending_interrupt = interrupt;
        }
        screen->text_sprites[0].pending_interrupt = interrupt;
        if (interrupt != 9) {
            screen->state = M_STATE_REPLAY_REFUSED;
        }
        screen->cursor = 0;
    }
    M_HighlightChoice(screen);
    if (screen->frame < 80) {
        return;
    }
    M_MoveCursorHorizontal(screen, 2);
    if (M_Pressed(M_CANCEL) || M_Pressed(M_MENU)) {
        M_Leave(screen);
        return;
    }
    if (M_Pressed(M_CONFIRM)) {
        if (screen->cursor == 0) {
            M_ChooseSlot(screen);
        } else {
            M_Leave(screen);
        }
    }
}

static void M_UpdateReplayName(M_SCREEN *const screen)
{
    if (screen->frame < 30) {
        return;
    }
    M_MoveCharIndex(screen);
    if (M_PressedOrRepeated(M_CONFIRM)) {
        const int32_t position = M_NamePosition(screen);
        if (screen->char_index < M_ALPHABET_SPACE) {
            screen->name[position] = m_NameAlphabet[screen->char_index];
        } else if (screen->char_index == M_ALPHABET_SPACE) {
            screen->name[position] = ' ';
        } else {
            M_SaveReplay(screen);
            screen->frame = 0;
            screen->state = M_STATE_LEAVE;
            M_Interrupt(screen, 2);
        }
        // The original goes on after the save as after any character.
        M_AdvanceNameCursor(screen);
    }
    if (M_PressedOrRepeated(M_CANCEL)) {
        M_EraseNameCharacter(screen, screen->name);
    }
    if (M_Pressed(M_MENU)) {
        M_ChooseSlot(screen);
    }
}

static void M_UpdateReplayOverwrite(M_SCREEN *const screen)
{
    M_HighlightChoice(screen);
    if (screen->frame < 20) {
        return;
    }
    M_MoveCursorHorizontal(screen, 2);
    if (M_Pressed(M_CANCEL) || M_Pressed(M_MENU)) {
        M_ChooseSlot(screen);
        return;
    }
    if (M_Pressed(M_CONFIRM)) {
        screen->frame = 0;
        if (screen->cursor == 0) {
            M_Interrupt(screen, 0xf);
            screen->sprites[screen->replay_slot + M_SLOT_SPRITE]
                .pending_interrupt = 0xe;
            screen->state = M_STATE_REPLAY_NAME;
        } else {
            M_ChooseSlot(screen);
        }
    }
}

static void M_UpdateReplaySaving(M_SCREEN *const screen)
{
    switch (screen->state) {
    case M_STATE_REPLAY_PROMPT:
        M_UpdateReplayPrompt(screen);
        break;
    case M_STATE_REPLAY_REFUSED:
        if (screen->frame >= 20
            && (M_Pressed(M_CONFIRM) || M_Pressed(M_CANCEL))) {
            M_Leave(screen);
        }
        break;
    case M_STATE_REPLAY_SLOTS:
        M_ListSlots(screen);
        break;
    case M_STATE_REPLAY_NAME:
        M_UpdateReplayName(screen);
        break;
    case M_STATE_REPLAY_OVERWRITE:
        M_UpdateReplayOverwrite(screen);
        break;
    default:
        break;
    }
}

static void M_UpdateStatistics(M_SCREEN *const screen)
{
    ANM_VM *const panel = &screen->sprites[0x25];
    if (screen->state == M_STATE_STATISTICS) {
        if (screen->frame <= 30) {
            panel->pending_interrupt = 0x10;
        }
        if (screen->frame >= 90 && M_Pressed(M_CONFIRM)) {
            panel->pending_interrupt = 2;
            screen->frame = 0;
            screen->state = M_STATE_STATISTICS_LEAVE;
        }
    } else if (screen->frame >= 30) {
        screen->frame = 59;
        screen->state = M_STATE_REPLAY_PROMPT;
    }
}

static void M_HighlightMenu(M_SCREEN *const screen)
{
    for (int32_t i = 0; i <= 6; i++) {
        ANM_VM *const sprite = &screen->sprites[1 + i];
        if (i == screen->cursor) {
            sprite->color = 0xff505050;
            sprite->pos_offset = (VEC3) { -4.0f, -4.0f, 0.0f };
        } else {
            sprite->color = 0xb0000000;
            sprite->pos_offset = (VEC3) {};
        }
    }
}

static void M_OpenMenu(M_SCREEN *const screen)
{
    if (screen->frame == 0) {
        M_Interrupt(screen, 1);
        for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
            screen->sprites[i].color_op = true;
            screen->sprites[i].color &= 0xff000000;
        }
        M_HighlightMenu(screen);
    }
}

static void M_UpdateMenu(M_SCREEN *const screen)
{
    M_MoveCursorVertical(screen, 7);
    M_HighlightMenu(screen);
    if (M_Pressed(M_CONFIRM)) {
        switch (screen->cursor) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            M_Interrupt(screen, (int16_t)(screen->cursor + 3));
            screen->difficulty = screen->cursor;
            screen->state = (M_STATE)(screen->cursor + 3);
            screen->list_state = screen->state;
            screen->frame = 0;
            screen->cursor = screen->saved_cursor;
            screen->character = -1;
            screen->spell_page = -1;
            break;
        case 5:
            M_Interrupt(screen, (int16_t)(screen->cursor + 3));
            screen->difficulty = screen->cursor;
            screen->state = M_STATE_SPELL_CARDS;
            screen->list_state = screen->state;
            screen->frame = 0;
            screen->character = -1;
            screen->cursor = screen->saved_spell_page;
            screen->spell_page = -1;
            break;
        case 6:
            M_Interrupt(screen, 2);
            screen->state = M_STATE_LEAVE;
            Sound_Queue(SOUND_CANCEL);
            break;
        default:
            break;
        }
    }
    if (M_Pressed(M_CANCEL)) {
        screen->cursor = 6;
        Sound_Queue(SOUND_CANCEL);
    }
}

static void M_ReturnToMenu(M_SCREEN *const screen, int32_t *const saved)
{
    Sound_Queue(SOUND_CANCEL);
    screen->state = M_STATE_OPEN_MENU;
    screen->frame = 1;
    M_Interrupt(screen, 1);
    *saved = screen->cursor;
    screen->cursor = screen->difficulty;
}

// Counts the button sequence that marks every stage cleared: five presses
// of unlock_start, two of quit and three of unlock_end, with focus or skip
// held. The next calc applies it.
static void M_UpdateUnlock(M_SCREEN *const screen)
{
    if ((g_Buttons.word & 0x104) == 0) {
        screen->cheat_progress = 0;
        return;
    }
    if (screen->cheat_progress >= 10) {
        for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
            for (int32_t k = 0; k < 5; k++) {
                g_GameManager.records.clears[shot].without_continues[k] =
                    M_CLEAR_STAGE;
                g_GameManager.records.clears[shot].with_continues[k] =
                    M_CLEAR_STAGE;
            }
        }
        screen->cheat_progress = 0;
        Sound_Queue(SOUND_EXTEND);
        return;
    }
    const uint16_t step = screen->cheat_progress < 5 ? 0x800
        : screen->cheat_progress < 7                 ? 0x200
                                                     : 0x400;
    if (M_Pressed(step)) {
        screen->cheat_progress++;
    } else if (M_Pressed(0x160b)) {
        screen->cheat_progress = 0;
    }
}

static void M_UpdateScores(M_SCREEN *const screen)
{
    if (screen->character != screen->cursor && screen->frame == 20) {
        screen->character = screen->cursor;
        M_RenderShotNames(screen);
    }
    if (screen->frame < 30) {
        return;
    }
    if (M_MoveCursorHorizontal(screen, 2)) {
        screen->frame = 0;
        M_Interrupt(screen, (int16_t)(screen->difficulty + 3));
    }
    if (M_Pressed(M_CANCEL)) {
        M_ReturnToMenu(screen, &screen->saved_cursor);
    }
}

static void M_UpdateSpellCards(M_SCREEN *const screen)
{
    if (screen->spell_page != screen->cursor && screen->frame == 20) {
        screen->spell_page = screen->cursor;
        const int32_t first = screen->spell_page * M_SPELL_PAGE_SIZE;
        for (int32_t i = first; i < first + M_SPELL_PAGE_SIZE; i++) {
            if (i >= SPELL_CARD_COUNT) {
                break;
            }
            const SPELL_CARD_RECORD *const card =
                &g_GameManager.records.spell_cards[i];
            Result_Log(
                Anm_RenderText(
                    &screen->text_sprites[i % M_SPELL_PAGE_SIZE], 0xffffff, 0,
                    "%s",
                    card->attempts == 0
                        ? g_Strings[STRING_RESULT_UNKNOWN_SPELL_CARD]
                        : Translation_GetSpellName(i, card->name)),
                LOG_LEVEL_WARNING);
        }
    }
    if (screen->frame < 30) {
        return;
    }
    if (M_MoveCursorHorizontal(screen, 7)) {
        screen->frame = 0;
        M_Interrupt(screen, (int16_t)(screen->difficulty + 3));
    }
    if (M_Pressed(M_CANCEL)) {
        M_ReturnToMenu(screen, &screen->saved_spell_page);
    }
}

static CHAIN_STATUS M_Update(void *const arg)
{
    M_SCREEN *const screen = arg;
    switch (screen->state) {
    case M_STATE_PRACTICE:
        Scene_Request(SCENE_TITLE);
        return CHAIN_REMOVE;
    case M_STATE_OPEN_MENU:
        M_OpenMenu(screen);
        if (screen->frame < 20) {
            break;
        }
        screen->state = M_STATE_MENU;
        screen->frame = 0;
        [[fallthrough]];
    case M_STATE_MENU:
        M_UpdateMenu(screen);
        break;
    case M_STATE_LEAVE:
        if (screen->frame < 60) {
            break;
        }
        Scene_Request(SCENE_TITLE);
        return CHAIN_REMOVE;
    case M_STATE_SCORES_EXTRA:
        M_UpdateUnlock(screen);
        [[fallthrough]];
    case M_STATE_SCORES_EASY:
    case M_STATE_SCORES_NORMAL:
    case M_STATE_SCORES_HARD:
    case M_STATE_SCORES_LUNATIC:
        M_UpdateScores(screen);
        break;
    case M_STATE_SPELL_CARDS:
        M_UpdateSpellCards(screen);
        break;
    case M_STATE_NAME_ENTRY:
        M_UpdateNameEntry(screen);
        break;
    case M_STATE_REPLAY_PROMPT:
    case M_STATE_REPLAY_REFUSED:
    case M_STATE_REPLAY_SLOTS:
    case M_STATE_REPLAY_NAME:
    case M_STATE_REPLAY_OVERWRITE:
        M_UpdateReplaySaving(screen);
        break;
    case M_STATE_STATISTICS:
    case M_STATE_STATISTICS_LEAVE:
        M_UpdateStatistics(screen);
        break;
    }
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        Anm_Step(&screen->sprites[i]);
    }
    screen->frame++;
    return CHAIN_CONTINUE;
}

static ANM_DRAW M_Draw(const uint32_t part)
{
    ANM_DRAW draw = Anm_ScreenDraw();
    draw.view = Anm_ScreenView(g_GameManager.camera.viewport, ANM_VIEW_SCREEN);
    draw.interpolate = true;
    draw.part = part;
    return draw;
}

static void M_DrawSprite(const ANM_VM *const vm, const uint32_t part)
{
    const ANM_DRAW draw = M_Draw(part);
    Anm_DrawUnrotated(vm, &draw);
}

// Returns the list of difficulty and shot. The original indexes its lists
// as one array, so a shot past the end of a row reaches into the next
// difficulty. Returns nullptr outside the array.
static const SCORE_LIST *M_List(
    const M_SCREEN *const screen, const int32_t difficulty, const int32_t shot)
{
    const int32_t index = difficulty * SCORE_SHOT_COUNT + shot;
    if (index < 0 || index >= SCORE_DIFFICULTY_COUNT * SCORE_SHOT_COUNT) {
        return nullptr;
    }
    return &screen->scores[index / SCORE_SHOT_COUNT][index % SCORE_SHOT_COUNT];
}

static void M_AddEntry(VEC3 pos, const HIGH_SCORE *const entry)
{
    char name[9];
    memcpy(name, entry->name, sizeof(name) - 1);
    name[8] = 0;
    if (entry->stage <= 6) {
        AsciiText_AddFormat(
            pos, g_Strings[STRING_RESULT_SCORE], name, entry->score,
            entry->stage);
    } else if (entry->stage == 7) {
        AsciiText_AddFormat(
            pos, g_Strings[STRING_RESULT_SCORE_EXTRA], name, entry->score);
    } else {
        AsciiText_AddFormat(
            pos, g_Strings[STRING_RESULT_SCORE_CLEAR], name, entry->score);
    }
}

static void M_AddNameCursor(
    const M_SCREEN *const screen, const VEC3 pos, const char *const format)
{
    char name[9] = "        ";
    name[M_NamePosition(screen)] = '_';
    AsciiText_AddFormat(pos, format, name);
}

static void M_DrawScores(M_SCREEN *const screen, VEC3 pos)
{
    screen->text_sprites[0].pos = pos;
    M_DrawSprite(&screen->text_sprites[0], 0x100);
    pos.x += 320.0f;
    screen->text_sprites[1].pos = pos;
    M_DrawSprite(&screen->text_sprites[1], 0x101);
    pos.x -= 320.0f;
    pos.y += 18.0f;
    pos.y += 18.0f;
    const SCORE_LIST *const left =
        M_List(screen, screen->difficulty, screen->character * 2);
    const SCORE_LIST *const right =
        M_List(screen, screen->difficulty, screen->character * 2 + 1);
    if (left == nullptr || right == nullptr) {
        return;
    }
    const bool naming = screen->state == M_STATE_NAME_ENTRY;
    const uint8_t shot_type = g_GameManager.run.shot_type;
    for (int32_t i = 0; i < SCORE_LIST_SIZE; i++) {
        const HIGH_SCORE *const first = &left->entries[i];
        const HIGH_SCORE *const second = &right->entries[i];
        if (!naming) {
            AsciiText_SetColor(0xffffc0c0);
        } else if (shot_type != 0) {
            AsciiText_SetColor(0x80ffc0c0);
        } else if (first->is_new) {
            AsciiText_SetColor(0xfff0f0ff);
            M_AddNameCursor(screen, pos, "   %8s");
        } else {
            AsciiText_SetColor(0x80ffffc0);
        }
        AsciiText_AddFormat(pos, g_Strings[STRING_RESULT_RANK], i + 1);
        pos.x += 36.0f;
        M_AddEntry(pos, first);
        pos.x += 300.0f;
        if (!naming) {
            AsciiText_SetColor(0xffc0c0ff);
        } else if (shot_type != 1) {
            AsciiText_SetColor(0x80c0c0ff);
        } else if (second->is_new) {
            AsciiText_SetColor(0xfffff0f0);
            M_AddNameCursor(screen, pos, "%8s");
        } else {
            AsciiText_SetColor(0xc0c0c0ff);
        }
        M_AddEntry(pos, second);
        pos.x -= 336.0f;
        pos.y += 18.0f;
    }
}

static void M_DrawSpellCards(M_SCREEN *const screen, VEC3 pos)
{
    pos.y += 16.0f;
    for (int32_t i = 0; i < M_SPELL_PAGE_SIZE; i++) {
        const int32_t index = screen->spell_page * M_SPELL_PAGE_SIZE + i;
        if (index >= SPELL_CARD_COUNT) {
            break;
        }
        if (index >= 0) {
            const SPELL_CARD_RECORD *const card =
                &g_GameManager.records.spell_cards[index];
            ANM_VM *const text = &screen->text_sprites[i];
            text->pos = pos;
            if (card->attempts == 0) {
                AsciiText_SetColor(0x80c0c0ff);
            } else if (card->captures == 0) {
                AsciiText_SetColor(0xffc0a0a0);
            } else {
                AsciiText_SetColor(0xfff0f0ffu - (uint32_t)i * 0x80800);
            }
            AsciiText_AddFormat(pos, "No.%.2d", index + 1);
            text->pos.x += 96.0f;
            M_DrawSprite(text, 0x100 + (uint32_t)i);
            pos.x += 368.0f;
            AsciiText_AddFormat(pos, "%3d/%3d", card->captures, card->attempts);
            pos.x -= 368.0f;
        }
        pos.y += 30.0f;
    }
}

static void M_DrawAlphabet(const M_SCREEN *const screen)
{
    VEC3 pos = { 160.0f, 356.0f, 0.0f };
    for (int32_t row = 0; row < 6; row++) {
        int32_t column;
        for (column = 0; column < 16; column++) {
            const int32_t index = row * 16 + column;
            float shift = 0.0f;
            if (screen->char_index == index) {
                AsciiText_SetColor(0xffffffc0);
                float scale;
                if (screen->frame % 64 < 32) {
                    scale = 0.8f * (float)(screen->frame % 32) / 32.0f + 1.2f;
                } else {
                    scale = 2.0f - 0.8f * (float)(screen->frame % 32) / 32.0f;
                }
                g_AsciiText.scale_x = scale;
                g_AsciiText.scale_y = scale;
                shift = -(scale - 1.0f) * 8.0f;
            } else {
                AsciiText_SetColor(0x60c0c0c0);
                g_AsciiText.scale_x = 1.0f;
                g_AsciiText.scale_y = 1.0f;
            }
            char glyph[2] = { (char)m_NameAlphabet[index], 0 };
            if (row == 5 && column == 14) {
                glyph[0] = (char)0x80;
            } else if (row == 5 && column == 15) {
                glyph[0] = (char)0x81;
            }
            AsciiText_AddString(
                (VEC3) { pos.x + shift, pos.y + shift, pos.z }, glyph);
            pos.x += 20.0f;
        }
        pos.x -= (float)(column * 20);
        pos.y += 18.0f;
    }
}

static void M_DrawSlots(M_SCREEN *const screen)
{
    for (int32_t i = 0; i < 6; i++) {
        M_DrawSprite(&screen->sprites[0xf + i], 0x200 + (uint32_t)i);
    }
    AsciiText_AddString(
        screen->sprites[0x15].pos, g_Strings[STRING_REPLAY_SAVE_HEADER]);
    const int32_t shot =
        g_GameManager.run.character * 2 + g_GameManager.run.shot_type;
    for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
        const VEC3 pos = screen->sprites[M_SLOT_SPRITE + i].pos;
        AsciiText_SetColor(i == screen->replay_slot ? 0xffff8080 : 0xff808080);
        const SAVED_REPLAY *const slot = &screen->slots[i];
        if (screen->state == M_STATE_REPLAY_NAME) {
            AsciiText_AddFormat(
                pos, g_Strings[STRING_REPLAY_SAVE_SLOT], i + 1,
                (const char *)screen->name, screen->replay_date,
                g_Strings[m_ShotTypeNames[shot]], screen->replay_score);
            AsciiText_SetColor(0xfff0f0ff);
            M_AddNameCursor(screen, pos, "      %8s");
        } else if (!slot->valid) {
            AsciiText_AddFormat(
                pos, g_Strings[STRING_REPLAY_SAVE_EMPTY], i + 1);
        } else {
            AsciiText_AddFormat(
                pos, g_Strings[STRING_REPLAY_SAVE_SLOT], i + 1,
                (const char *)slot->name, slot->summary.date,
                g_Strings[m_ShotTypeNames[slot->summary.shot]],
                slot->summary.score);
        }
    }
}

static void M_DrawStatistics(const M_SCREEN *const screen)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    const ANM_VM *const panel = &screen->sprites[0x25];
    AsciiText_SetColor(panel->color);
    const float progress = gm->run.difficulty < 4
        ? (float)gm->run.progress_frames / 89500.0f
        : (float)gm->run.progress_frames / 39600.0f;
    VEC3 pos = panel->pos;
    pos.x += 224.0f;
    pos.y += 32.0f;
    AsciiText_AddFormat(pos, "%9d", gm->score.shown);
    pos.y += 22.0f;
    AsciiText_AddString(pos, g_Strings[m_DifficultyNames[gm->run.difficulty]]);
    pos.y += 22.0f;
    if (gm->run.difficulty == 0 || !gm->mode.game_cleared) {
        AsciiText_AddFormat(pos, "    %3.2f%%", (double)(progress * 100.0f));
    } else {
        AsciiText_AddString(pos, "      100%");
    }
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", gm->stats.continues_used);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", gm->stats.deaths);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", gm->stats.bombs_used);
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "%9d", gm->stats.spell_cards_captured);
    const RESULT_SCREEN_CLOCK *const clock = ResultSaves_GetClock();
    float slowdown =
        (clock->achieved_frames / clock->expected_frames - 0.5f) * 2.0f;
    if (slowdown < 0.0f) {
        slowdown = 0.0f;
    } else if (slowdown >= 1.0f) {
        slowdown = 1.0f;
    }
    slowdown = (1.0f - slowdown) * 100.0f;
    pos.y += 22.0f;
    AsciiText_AddFormat(pos, "    %3.2f%%", (double)slowdown);
    AsciiText_SetColor(0xffffffff);
}

static CHAIN_STATUS M_DrawScreen(void *const arg)
{
    M_SCREEN *const screen = arg;
    VIEWPORT *const viewport = &g_GameManager.camera.viewport;
    viewport->x = 0.0f;
    viewport->y = 0.0f;
    viewport->width = 640.0f;
    viewport->height = 480.0f;
    Anm_CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        ANM_VM sprite = screen->sprites[i];
        sprite.pos = Vec3_Add(sprite.pos, sprite.pos_offset);
        M_DrawSprite(&sprite, (uint32_t)i);
    }
    const ANM_VM *const board = &screen->sprites[0xe];
    if (board->pos.x < 640.0f) {
        if (screen->list_state != M_STATE_SPELL_CARDS) {
            M_DrawScores(screen, board->pos);
        } else {
            M_DrawSpellCards(screen, board->pos);
        }
    }
    if (screen->state == M_STATE_NAME_ENTRY
        || screen->state == M_STATE_REPLAY_NAME) {
        M_DrawAlphabet(screen);
    }
    g_AsciiText.scale_x = 1.0f;
    g_AsciiText.scale_y = 1.0f;
    if (screen->state >= M_STATE_REPLAY_PROMPT
        && screen->state <= M_STATE_REPLAY_OVERWRITE) {
        M_DrawSlots(screen);
    }
    AsciiText_SetColor(0xffffffff);
    if (screen->state == M_STATE_STATISTICS
        || screen->state == M_STATE_STATISTICS_LEAVE) {
        M_DrawStatistics(screen);
    }
    return CHAIN_CONTINUE;
}

static RESULT M_LoadGraphics(M_SCREEN *const screen)
{
    MUST(Anm_LoadSurface(0, "data/result/result.jpg"));
    MUST(Anm_Load(0x25, "data/result00.anm", 0x100));
    MUST(Anm_Load(0x26, "data/result01.anm", 0x108));
    MUST(Anm_Load(0x27, "data/result02.anm", 0x10f));
    MUST(Anm_Load(0x28, "data/result03.anm", 0x125));
    for (int32_t i = 0; i < M_SPRITE_COUNT; i++) {
        ANM_VM *const sprite = &screen->sprites[i];
        Anm_CreateVm(sprite);
        Anm_StartScript(sprite, M_FIRST_SCRIPT + i);
    }
    for (int32_t i = 0; i < M_TEXT_SPRITE_COUNT; i++) {
        ANM_VM *const sprite = &screen->text_sprites[i];
        Anm_CreateVm(sprite);
        Anm_InitVm(sprite);
        Anm_SetSprite(sprite, M_FIRST_TEXT_SPRITE + i);
        sprite->anchor = 3;
        sprite->font_width = 15;
        sprite->font_height = 15;
    }
    return OK;
}

static RESULT M_Added(void *const arg)
{
    M_SCREEN *const screen = arg;
    GAME_MANAGER *const gm = &g_GameManager;
    if (screen->state != M_STATE_PRACTICE) {
        MUST(M_LoadGraphics(screen));
    }
    for (int32_t d = 0; d < SCORE_DIFFICULTY_COUNT; d++) {
        for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
            ScoreList_InitDefaults(&screen->scores[d][shot], d);
        }
    }
    screen->saved_cursor = 0;
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    screen->score_loaded = true;
    for (int32_t d = 0; d < SCORE_DIFFICULTY_COUNT; d++) {
        for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
            ScoreFile_LoadHighScores(file, &screen->scores[d][shot], shot, d);
        }
    }
    if (screen->state != M_STATE_NAME_ENTRY
        && screen->state != M_STATE_PRACTICE) {
        ScoreFile_LoadSpellCards(file, gm->records.spell_cards);
        ScoreFile_LoadClears(file, gm->records.clears);
        ScoreFile_LoadPractice(file, gm->records.practice);
    }
    if (screen->state == M_STATE_PRACTICE) {
        const int32_t shot = gm->run.character * 2 + gm->run.shot_type;
        const int32_t stage = gm->run.stage - 1;
        FAIL_IF(
            stage < 0 || stage >= SCORE_STAGE_COUNT
                || gm->run.difficulty >= SCORE_PRACTICE_DIFFICULTY_COUNT,
            "no practice score for stage %d, difficulty %d", gm->run.stage,
            gm->run.difficulty);
        PRACTICE_RECORD *const record =
            &gm->records.practice[shot][stage][gm->run.difficulty];
        if (record->score < gm->score.value) {
            record->score = gm->score.value;
        }
    }
    return OK;
}

static void M_SaveScoreFile(const M_SCREEN *const screen)
{
    const GAME_MANAGER *const gm = &g_GameManager;
    const uint8_t key = (uint8_t)Rng_U16InRange(&g_Rng, 256);
    const uint8_t random = (uint8_t)Rng_U16InRange(&g_Rng, 256);
    const SCORE_RECORDS records = {
        .spell_cards = gm->records.spell_cards,
        .clears = gm->records.clears,
        .practice = gm->records.practice,
    };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    ScoreFile_Write(
        &writer, ResultSaves_GetScoreFile(), screen->scores, &records, key,
        random);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    ResultSaves_PutScoreFile(data, size);
}

static void M_Deleted(void *const arg)
{
    M_SCREEN *const screen = arg;
    if (screen->score_loaded) {
        M_SaveScoreFile(screen);
    }
    screen->score_loaded = false;
    Anm_Release(0x25);
    Anm_Release(0x26);
    Anm_Release(0x27);
    Anm_Release(0x28);
    Anm_ReleaseSurface(0);
    Chain_Remove(&screen->draw);
}

RESULT ResultScreen_Register(const int32_t mode)
{
    M_SCREEN *const screen = &m_Screen;
    *screen = (M_SCREEN) { .cursor = 1 };
    Chain_InitElem(&screen->calc, M_Update, screen);
    screen->calc.added = M_Added;
    screen->calc.deleted = M_Deleted;
    if (mode != 0) {
        screen->state = g_GameManager.mode.in_practice ? M_STATE_PRACTICE
                                                       : M_STATE_NAME_ENTRY;
    }
    MUST(Chain_AddCalc(&g_Chain, &screen->calc, M_CALC_PRIORITY));
    Chain_InitElem(&screen->draw, M_DrawScreen, screen);
    return Chain_AddDraw(&g_Chain, &screen->draw, M_DRAW_PRIORITY);
}
