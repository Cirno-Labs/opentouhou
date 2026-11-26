#include "th02/score_entry/logic.h"

#include "th02/hud.h"
#include "th02/score_entry/render.h"
#include "th02/wait.h"

#include "core/base/assert.h"

#include <string.h>

typedef enum {
    SCORE_ENTRY_READ,
    SCORE_ENTRY_TAIL,
    SCORE_ENTRY_UNQUALIFIED,
    SCORE_ENTRY_DONE,
} SCORE_ENTRY_PHASE;

typedef struct {
    SCORE_SECTION *section;
    uint16_t keys;
    uint16_t input_locked;
    uint8_t input_delay;
    uint8_t col;
    uint8_t row;
    uint8_t name_pos;
    int8_t place;
    SCORE_ENTRY_PHASE phase;
} SCORE_ENTRY_MENU;

typedef struct {
    SCORE_ENTRY_EVENTS event;
    WAIT wait;
} SCORE_ENTRY;

static const uint8_t m_Alphabet[3][17] = {
    { 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5,
      0xb7, 0xb6, 0xb8, 0xb9, 0xba },
    { 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xda, 0xdb, 0xdc,
      0xdd, 0xde, 0xe0, 2, 3 },
    { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xe1, 0xe2,
      0xe3, 0xe4, 0xe5, 0xe6, 0xe7 },
};

static SCORE_ENTRY_MENU m_Menu;
static SCORE_ENTRY m_Entry;

static void M_Alphabet(const uint16_t attribute)
{
    ScoreEntryRender_Alphabet(m_Menu.col, m_Menu.row, attribute);
}

static void M_Name(void)
{
    ScoreEntryRender_Name(m_Menu.section, m_Menu.place, m_Menu.name_pos);
}

static void M_Move(const bool vertical, const int direction)
{
    M_Alphabet(TRAM_WHITE);
    uint8_t *const coord = vertical ? &m_Menu.row : &m_Menu.col;
    const int maximum = vertical ? 2 : 16;
    const int next = *coord + direction;
    *coord = next < 0 ? maximum : next > maximum ? 0 : next;
    M_Alphabet(0x85);
}

static SCORE_ENTRY_EVENTS M_Exit(void)
{
    m_Menu.phase = SCORE_ENTRY_DONE;
    return (SCORE_ENTRY_EVENTS) { SCORE_ENTRY_COMPLETE, true };
}

int32_t ScoreEntry_MainScore(
    const int32_t internal_score, const uint16_t continues)
{
    return (int32_t)((uint32_t)internal_score * 10u + continues);
}

uint8_t ScoreEntry_Alphabet(const uint8_t col, const uint8_t row)
{
    ASSERT(col < 17 && row < 3);
    return m_Alphabet[row][col];
}

static void M_MenuInit(void)
{
    memset(&m_Menu, 0, sizeof(m_Menu));
}

static SCORE_ENTRY_EVENTS M_MenuBegin(
    SCORE_SECTION *const section, const SCORE_RUN *const run, const bool ending)
{
    M_MenuInit();
    m_Menu.section = section;
    m_Menu.place = -1;
    if (section->records[SCORES_PLACES - 1].score <= run->score) {
        int place = SCORES_PLACES - 1;
        while (place > 0 && section->records[place - 1].score <= run->score) {
            place--;
        }
        for (int shift = SCORES_PLACES - 1; shift > place; shift--) {
            SCORE_RECORD *const dest = &section->records[shift];
            const SCORE_RECORD *const src = &section->records[shift - 1];
            dest->score = src->score;
            memcpy(dest->name_glyphs, src->name_glyphs, SCORES_NAME_GLYPHS - 1);
            dest->stage = src->stage;
            dest->year = src->year;
            dest->month = src->month;
            dest->day = src->day;
            dest->shot_type = src->shot_type;
        }
        SCORE_RECORD *const record = &section->records[place];
        record->score = run->score;
        record->stage = run->stage;
        record->year = run->year;
        record->day = run->day;
        record->month = run->month;
        record->shot_type = run->shot_type;
        memset(record->name_glyphs, 0xda, SCORES_NAME_GLYPHS - 1);
        m_Menu.place = place;
    }
    ScoreEntryRender_Table(section, ending, m_Menu.place);
    m_Menu.input_locked = 1;
    m_Menu.phase =
        m_Menu.place < 0 ? SCORE_ENTRY_UNQUALIFIED : SCORE_ENTRY_READ;
    return (SCORE_ENTRY_EVENTS) { m_Menu.place < 0 ? SCORE_ENTRY_WAIT_KEY
                                                   : SCORE_ENTRY_INPUT,
                                  false };
}

static SCORE_ENTRY_EVENTS M_MenuStep(const uint16_t keys)
{
    if (m_Menu.phase == SCORE_ENTRY_DONE) {
        return (SCORE_ENTRY_EVENTS) { SCORE_ENTRY_COMPLETE, false };
    }
    if (m_Menu.phase == SCORE_ENTRY_UNQUALIFIED) {
        m_Menu.phase = SCORE_ENTRY_DONE;
        return (SCORE_ENTRY_EVENTS) { SCORE_ENTRY_COMPLETE, false };
    }
    if (m_Menu.phase == SCORE_ENTRY_TAIL) {
        m_Menu.input_locked = m_Menu.keys;
        if (m_Menu.input_locked != 0) {
            m_Menu.input_delay++;
            if (m_Menu.input_delay > 30 && (m_Menu.input_delay & 1) == 0) {
                m_Menu.input_locked = 0;
            }
        } else {
            m_Menu.input_delay = 0;
        }
        m_Menu.phase = SCORE_ENTRY_READ;
        return (SCORE_ENTRY_EVENTS) { SCORE_ENTRY_INPUT, false };
    }
    ASSERT(m_Menu.row < 3 && m_Menu.col < 17 && m_Menu.name_pos < 6);
    m_Menu.keys = keys;
    if (m_Menu.input_locked == 0) {
        if ((keys & GAME_KEY_UP) != 0) {
            M_Move(true, -1);
        }
        if ((keys & GAME_KEY_DOWN) != 0) {
            M_Move(true, 1);
        }
        if ((keys & GAME_KEY_LEFT) != 0) {
            M_Move(false, -1);
        }
        if ((keys & GAME_KEY_RIGHT) != 0) {
            M_Move(false, 1);
        }
        uint8_t *const name = m_Menu.section->records[m_Menu.place].name_glyphs;
        if ((keys & (GAME_KEY_SHOT | GAME_KEY_OK)) != 0) {
            if (m_Menu.row != 2 || m_Menu.col < 13) {
                name[m_Menu.name_pos] =
                    ScoreEntry_Alphabet(m_Menu.col, m_Menu.row);
                if (m_Menu.name_pos == 5) {
                    M_Alphabet(TRAM_WHITE);
                    m_Menu.col = 16;
                    m_Menu.row = 2;
                    M_Alphabet(0x85);
                }
                if (m_Menu.name_pos < 5) {
                    m_Menu.name_pos++;
                }
            } else if (m_Menu.col == 13) {
                name[m_Menu.name_pos] = 0xcf;
                if (m_Menu.name_pos < 5) {
                    m_Menu.name_pos++;
                }
            } else if (m_Menu.col == 14) {
                if (m_Menu.name_pos > 0) {
                    m_Menu.name_pos--;
                }
                name[m_Menu.name_pos] = 0xcf;
            } else if (m_Menu.col == 15) {
                if (m_Menu.name_pos < 5) {
                    m_Menu.name_pos++;
                }
            } else {
                return M_Exit();
            }
            M_Name();
        }
        if ((keys & GAME_KEY_BOMB) != 0) {
            name[m_Menu.name_pos] = 0xcf;
            if (m_Menu.name_pos > 0) {
                m_Menu.name_pos--;
            }
            M_Name();
        }
        if ((keys & GAME_KEY_CANCEL) != 0) {
            return M_Exit();
        }
    }
    m_Menu.phase = SCORE_ENTRY_TAIL;
    return (SCORE_ENTRY_EVENTS) { SCORE_ENTRY_FRAME, false };
}

static SCORE_ENTRY_EVENTS M_Complete(void)
{
    const SCORE_ENTRY_EVENTS event = m_Entry.event;
    m_Entry.event.save_section = false;
    return event;
}

void ScoreEntry_Init(void)
{
    memset(&m_Entry, 0, sizeof(m_Entry));
    M_MenuInit();
}

void ScoreEntry_Begin(
    SCORE_SECTION *const section, const SCORE_RUN *const run, const bool ending,
    const TH02_INPUT *const input)
{
    ScoreEntry_Init();
    Hud_WipePlayfield();
    m_Entry.event = M_MenuBegin(section, run, ending);
    if (m_Entry.event.result == SCORE_ENTRY_WAIT_KEY) {
        Wait_Key(&m_Entry.wait, input);
    }
}

SCORE_ENTRY_EVENTS ScoreEntry_Update(const TH02_INPUT *const input)
{
    for (;;) {
        switch (m_Entry.event.result) {
        case SCORE_ENTRY_INPUT:
            m_Entry.event = M_MenuStep(GameInput_Sense(input));
            if (m_Entry.event.result == SCORE_ENTRY_COMPLETE) {
                return M_Complete();
            }
            Wait_Delay(&m_Entry.wait, 1);
            return m_Entry.event;
        case SCORE_ENTRY_FRAME:
            if (!Wait_Ready(&m_Entry.wait, nullptr, 0)) {
                return m_Entry.event;
            }
            m_Entry.event = M_MenuStep(0);
            break;
        case SCORE_ENTRY_WAIT_KEY:
            if (Wait_Ready(&m_Entry.wait, input, 0)) {
                m_Entry.event = M_MenuStep(0);
                return M_Complete();
            }
            return m_Entry.event;
        case SCORE_ENTRY_COMPLETE:
            return M_Complete();
        }
    }
}
