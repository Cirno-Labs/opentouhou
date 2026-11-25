#include "th01/score.h"

#include "th01/scale2x.h"
#include "th01/session.h"
#include "core/base/log.h"

#include <string.h>

typedef enum {
    SCORE_FADE_WAIT,
    SCORE_ENTRY_INPUT,
    SCORE_ENTRY_DELAY,
    SCORE_RELEASE,
    SCORE_DONE,
} SCORE_PHASE;

typedef struct {
    PALETTE_STATE palette;
    PALETTE_FADE fade;
    VSYNC_WAIT wait;
    SCORE_CONTEXT context;
    SCORE_PHASE phase;
    int32_t rank, place, row, column, cursor, inputs;
    char slots[8][9];
} SCORE_SCENE;

static SCORE_SAVES m_Saves;
static SCORE_SCENE m_Registration;

static const SCORE_ASSETS *m_ScoreAssets;

static const char *const m_RankLabels[] = {
    "　イージー　",
    "　ノーマル　",
    "　ハード　　",
    "ルナティック",
};
static const char *const m_Places[] = {
    "　靈　神　", "太元帥明王", "　天　仙　", "　神　仙　", "　地　仙　",
    "　人　仙　", "　仙　女　", "陰　陽　師", "靈　媒　師", "修　業　者",
};
static const char *const m_Symbols[] = {
    "！", "？", "＃", "＆", "＊", "§", "☆",  "◆",  "∞",
    "∴",  "♂",  "♀",  "…",  "“",  "”", "／", "．", "・",
};
static const char *const m_Headers[] = {
    "　霊　位　",
    "　　名　　前　　",
    "　　得　　点　　",
    "ステージ・ルート",
};
static const char m_Title[] = "東方靈異伝　強者の記録　";
static const char m_Digits[][4] = {
    "０", "１", "２", "３", "４", "５", "６", "７", "８", "９",
};

static void M_Character(const uint32_t codepoint, char out[9])
{
    out[0] = (char)(0xe0 | (codepoint >> 12));
    out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[2] = (char)(0x80 | (codepoint & 0x3f));
    out[3] = '\0';
}

static void M_Cell(const int32_t row, const int32_t column, char out[9])
{
    if (row < 4) {
        M_Character(
            (row < 2 ? 0xff41u : 0xff21u) + (uint32_t)((row % 2) * 18 + column),
            out);
    } else if (row == 4) {
        strcpy(out, m_Symbols[column]);
    } else if (column < 10) {
        strcpy(out, m_Digits[column]);
    } else {
        strcpy(
            out,
            column == 10       ? "SP"
                : column == 15 ? "←"
                : column == 16 ? "→"
                               : "終");
    }
}

static void M_TableText(
    const char *const text, const int32_t left, const int32_t top,
    const uint8_t color, const GAME_TEXT_WEIGHT weight, const bool reverse,
    const bool clear)
{
    const GAME_TEXT_STYLE style = { .weight = weight,
                                    .reverse = reverse,
                                    .clear_background = clear };
    GameText_Draw(&m_ScoreAssets->font, text, left, top, color, &style);
}

static void M_EntryScore(
    const int32_t value, const int32_t digits, const int32_t left,
    const int32_t top, const uint8_t color, const GAME_TEXT_WEIGHT weight)
{
    char full[96] = "";
    uint32_t divisor = 1;
    for (int32_t i = 1; i < digits; i++) {
        divisor *= 10;
    }
    bool put = false;
    for (int32_t i = 0; i < digits; i++) {
        const uint32_t digit = ((uint32_t)value / divisor) % 10;
        put |= digit != 0;
        strcat(full, put ? m_Digits[digit] : "　");
        divisor /= 10;
    }
    M_TableText(full, left, top, color, weight, false, false);
}

static void M_Alphabet(
    const int32_t row, const int32_t column, const bool selected)
{
    const int32_t left = 32 + column * 32, top = 240 + row * 24;
    Video_Restore16((GFX_RECT) { left, top, 32, 16 });
    char text[9];
    M_Cell(row, column, text);
    M_TableText(
        text, left, top, selected ? 3 : 7, GAME_TEXT_BOLD, selected, false);
}

static void M_Name(void)
{
    const int32_t top = 64 + m_Registration.place * 16;
    Video_Restore16((GFX_RECT) { 144, top, 128, 16 });
    for (int32_t i = 0; i < 8; i++) {
        M_TableText(
            m_Registration.slots[i], 144 + i * 16, top, 3, GAME_TEXT_BOLD,
            false, false);
    }
    M_TableText(
        "＿", 144 + m_Registration.cursor * 16, top, 3, GAME_TEXT_NORMAL, false,
        false);
}

static void M_Initial(void)
{
    const bool gameover = m_Registration.context == SCORE_GAMEOVER;
    Video_SelectDrawingPage(1);
    M_TableText(m_Title, 0, gameover ? 384 : 0, 7, GAME_TEXT_BOLD, false, true);
    M_TableText(
        m_RankLabels[m_Registration.rank], 192, gameover ? 384 : 0, 7,
        GAME_TEXT_BOLD, false, true);
    Video_SelectDrawingPage(0);
    Scale2x_Copy(48, 0, 0, gameover ? 384 : 0, 288, 16);
    if (gameover) {
        Video_Copy(0, (GFX_RECT) { 0, 384, 288, 16 }, 1, 0, 384);
    }
    static const int32_t columns[] = { 32, 144, 352, 512 };
    for (int32_t i = 0; i < 4; i++) {
        M_TableText(
            m_Headers[i], columns[i], 48, 3, GAME_TEXT_BLACK, false, false);
    }
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        const SCORE_ENTRY *const e =
            &m_Saves.tables[m_Registration.rank].entries[i];
        const uint8_t color = i == m_Registration.place ? 3 : 7;
        const int32_t top = 64 + i * 16;
        M_TableText(m_Places[i], 32, top, color, GAME_TEXT_BOLD, false, false);
        M_TableText(
            i == m_Registration.place ? "＿＿＿＿＿＿＿＿" : e->name, 144, top,
            color, GAME_TEXT_BOLD, false, false);
        M_EntryScore(e->score, 7, 352, top, color, GAME_TEXT_BLACK);
        if (e->stage < 40) {
            M_EntryScore(e->stage, 2, 528, top, color, GAME_TEXT_BOLD);
        } else if (e->stage == 50 || e->stage == 60) {
            M_TableText(
                e->stage == 50 ? "魔界" : "地獄", 528, top, color,
                GAME_TEXT_BOLD, false, false);
        }
        M_TableText("－", 560, top, color, GAME_TEXT_BOLD, false, false);
        M_TableText(e->route, 576, top, color, GAME_TEXT_BOLD, false, false);
    }
    GameInput_ResetGame();
    if (m_Registration.place == SCORE_PLACES) {
        g_GameInput.ok = true;
        g_GameInput.shot = true;
        m_Registration.phase = SCORE_RELEASE;
        return;
    }
    for (int32_t row = 0; row < 6; row++) {
        for (int32_t col = 0; col < 18; col++) {
            if (((row == 1 || row == 3) && col >= 8)
                || (row == 5 && col > 10 && col < 15)) {
                continue;
            }
            M_Alphabet(row, col, row == 0 && col == 0);
        }
    }
    m_Registration.phase = SCORE_ENTRY_INPUT;
}

static void M_Complete(void)
{
    SCORE_ENTRY *const entry =
        &m_Saves.tables[m_Registration.rank].entries[m_Registration.place];
    entry->name[0] = '\0';
    for (int32_t i = 0; i < 8; i++) {
        strcat(entry->name, m_Registration.slots[i]);
    }
    g_GameInput.ok = false;
    ScoreData_Queue(&m_Saves, m_Registration.rank);
    m_Registration.phase = SCORE_DONE;
}

static bool M_Shot(const int32_t row, const int32_t column)
{
    const bool was_last = m_Registration.cursor == 7;
    if (row == 5 && column >= 15) {
        if (column == 17) {
            return true;
        }
        if (column == 15 && m_Registration.cursor > 0) {
            m_Registration.cursor--;
        } else if (column == 16 && m_Registration.cursor < 7) {
            m_Registration.cursor++;
        }
    } else {
        if (row == 5 && column == 10) {
            strcpy(m_Registration.slots[m_Registration.cursor], "　");
        } else {
            M_Cell(row, column, m_Registration.slots[m_Registration.cursor]);
        }
        if (m_Registration.cursor < 7) {
            m_Registration.cursor++;
        }
    }
    const bool jump = was_last && m_Registration.context == SCORE_GAMEOVER
        && !(row == 5 && column >= 15);
    M_Name();
    if (jump) {
        M_Alphabet(m_Registration.row, m_Registration.column, false);
        m_Registration.row = 5;
        m_Registration.column = 17;
        M_Alphabet(m_Registration.row, m_Registration.column, true);
    }
    return false;
}

RESULT Score_LoadAssets(
    VFS *const vfs, VFS *const user_files, const FONT_ROM *const rom,
    const ALLOCATOR *const allocator, SCORE_ASSETS *const out)
{
    Result_Log(ScoreData_Load(vfs, user_files, &m_Saves), LOG_LEVEL_WARNING);
    *out = (SCORE_ASSETS) {};
    READER reader;
    MUST(Vfs_Open(vfs, "GAME_O.GRP", VFS_IN_MEMORY, &reader));
    const RESULT image = Pi_Load(&reader, allocator, &out->gameover);
    Reader_Close(&reader);
    MUST(image);
    FAIL_IF(
        out->gameover.width != VIDEO_WIDTH
            || out->gameover.height != VIDEO_HEIGHT,
        "GAME_O.GRP must be a 640 by 400 image");
    char characters[8192] =
        "東方靈異伝　強者の記録イージーノーマルハードルナティック"
        "霊位名前得点ステージ・ルート靈神太元帥明王天仙地人女陰陽師媒修業者"
        "＿＿＿＿＿＿＿＿魔界地獄－社無完 "
        "ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ"
        "ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ"
        "０１２３４５６７８９！？＃＆＊§☆◆∞∴♂♀…“”／．・SP←→終";
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        for (int32_t i = 0; i < SCORE_PLACES; i++) {
            strcat(characters, m_Saves.tables[rank].entries[i].name);
            strcat(characters, m_Saves.tables[rank].entries[i].route);
        }
    }
    return GameText_Load(rom, characters, allocator, &out->font);
}

bool Score_OnInput(void)
{
    const int32_t old_row = m_Registration.row,
                  old_column = m_Registration.column;
    if (g_GameInput.up) {
        m_Registration.row =
            m_Registration.row == 0 ? 5 : m_Registration.row - 1;
        if ((m_Registration.row == 1 || m_Registration.row == 3)
            && m_Registration.column >= 8) {
            m_Registration.row--;
        }
        if (m_Registration.row == 5 && m_Registration.column > 10
            && m_Registration.column < 15) {
            m_Registration.row = 4;
        }
        g_GameInput.up = false;
    } else if (g_GameInput.down) {
        m_Registration.row =
            m_Registration.row == 5 ? 0 : m_Registration.row + 1;
        if ((m_Registration.row == 1 || m_Registration.row == 3)
            && m_Registration.column >= 8) {
            m_Registration.row++;
        }
        if (m_Registration.row == 5 && m_Registration.column > 10
            && m_Registration.column < 15) {
            m_Registration.row = 0;
        }
        g_GameInput.down = false;
    } else if (g_GameInput.horizontal == GAME_INPUT_LEFT) {
        m_Registration.column =
            m_Registration.column == 0 ? 17 : m_Registration.column - 1;
        if ((m_Registration.row == 1 || m_Registration.row == 3)
            && m_Registration.column >= 8) {
            m_Registration.column = 7;
        }
        if (m_Registration.row == 5 && m_Registration.column > 10
            && m_Registration.column < 15) {
            m_Registration.column = 10;
        }
        g_GameInput.horizontal = 0;
    } else if (g_GameInput.horizontal == GAME_INPUT_RIGHT) {
        m_Registration.column =
            m_Registration.column == 17 ? 0 : m_Registration.column + 1;
        if ((m_Registration.row == 1 || m_Registration.row == 3)
            && m_Registration.column >= 8) {
            m_Registration.column = 0;
        }
        if (m_Registration.row == 5 && m_Registration.column > 10
            && m_Registration.column < 15) {
            m_Registration.column = 15;
        }
        g_GameInput.horizontal = 0;
    } else if (g_GameInput.shot) {
        if (M_Shot(m_Registration.row, m_Registration.column)) {
            return true;
        }
        g_GameInput.shot = false;
        return false;
    } else if (g_GameInput.strike) {
        M_Shot(5, 15);
        g_GameInput.strike = false;
        return false;
    } else {
        return false;
    }
    M_Alphabet(old_row, old_column, false);
    M_Alphabet(m_Registration.row, m_Registration.column, true);
    return false;
}

void Score_Begin(
    const SCORE_ASSETS *const assets, const SCORE_CONTEXT context,
    const int32_t rank, const SCORE_ENTRY *const entry)
{
    memset(&m_Registration, 0, sizeof(m_Registration));
    m_ScoreAssets = assets;
    m_Registration.context = context;
    m_Registration.rank = rank;
    SCORE_ENTRY inserted = *entry;
    strcpy(inserted.name, "                ");
    m_Registration.place = ScoreData_Insert(&m_Saves.tables[rank], &inserted);
    for (int32_t i = 0; i < 8; i++) {
        strcpy(m_Registration.slots[i], "  ");
    }
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    if (context == SCORE_ENDING) {
        M_Initial();
        return;
    }
    Video_SelectDrawingPage(0);
    Video_Clear(0);
    Palette_ShowSolid(0, 0, 0);
    const PI_IMAGE *const image = &assets->gameover;
    memcpy(g_Video.pages[1], image->pixels, VIDEO_PAGE_SIZE);
    memcpy(g_Video.pages[0], image->pixels, VIDEO_PAGE_SIZE);
    Palette_Init(&m_Registration.palette);
    Palette_Import(&m_Registration.palette.saved, image->palette);
    Palette_BeginFade(
        &m_Registration.fade, &m_Registration.palette, PALETTE_FADE_BLACK_IN);
    VSyncIrq_BeginWait(&m_Registration.wait);
    m_Registration.phase = SCORE_FADE_WAIT;
}

bool Score_Tick(const TH01_INPUT *const raw, const bool reference_edge)
{
    if (reference_edge) {
        VSyncIrq_Edge();
    }
    for (;;) {
        switch (m_Registration.phase) {
        case SCORE_FADE_WAIT:
            if (!VSyncIrq_WaitReady(&m_Registration.wait)) {
                return false;
            }
            Palette_FadeStep(&m_Registration.fade, &m_Registration.palette);
            if (m_Registration.fade.remaining != 0) {
                VSyncIrq_BeginWait(&m_Registration.wait);
                return false;
            }
            M_Initial();
            break;
        case SCORE_ENTRY_DELAY:
            if (m_Registration.context == SCORE_ENDING
                    ? (uint16_t)g_VSync.count1 < 4
                    : !VSyncIrq_DelayReady(4)) {
                return false;
            }
            m_Registration.inputs++;
            m_Registration.phase = SCORE_ENTRY_INPUT;
            break;
        case SCORE_ENTRY_INPUT: {
            if (m_Registration.context == SCORE_ENDING) {
                GameInput_SenseEnding(raw);
            } else {
                GameInput_SenseGame(raw, false);
            }
            const bool action = g_GameInput.up || g_GameInput.down
                || g_GameInput.horizontal == GAME_INPUT_LEFT
                || g_GameInput.horizontal == GAME_INPUT_RIGHT
                || g_GameInput.shot || g_GameInput.strike;
            const bool enter = Score_OnInput();
            if (enter || g_GameInput.ok
                || (m_Registration.context == SCORE_GAMEOVER
                    && m_Registration.inputs > 1000)) {
                M_Complete();
                return true;
            }
            if (!action) {
                return false;
            }
            if (m_Registration.context == SCORE_ENDING) {
                g_VSync.count1 = 0;
            }
            m_Registration.phase = SCORE_ENTRY_DELAY;
            break;
        }
        case SCORE_RELEASE:
            if (m_Registration.context == SCORE_ENDING) {
                GameInput_SenseEnding(raw);
            } else {
                GameInput_SenseGame(raw, false);
            }
            if (!g_GameInput.ok || !g_GameInput.shot) {
                m_Registration.phase = SCORE_DONE;
                return true;
            }
            return false;
        case SCORE_DONE:
            return true;
        }
    }
}

void Score_Close(void)
{
    memset(&m_Registration, 0, sizeof(m_Registration));
    m_ScoreAssets = nullptr;
}

void Score_SyncSession(void)
{
    g_Session.table_high_score =
        (uint32_t)m_Saves.tables[g_Session.settings.difficulty]
            .entries[0]
            .score;
}

RESULT Score_FlushSaves(void)
{
    return ScoreData_WriteSaves(&m_Saves);
}
