#include "th02/dialogue/assets.h"

#include "core/base/assert.h"

#include <string.h>

static RESULT M_Boxes(READER *const reader, DIALOGUE_ASSETS *const out)
{
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size % 80 != 0 || size > 64 * 80, "Invalid dialogue text length");
    out->count = (uint8_t)(size / 80);
    if (out->count != 0) {
        out->boxes =
            Allocator_AllocZ(&out->allocator, out->count * sizeof(*out->boxes));
    }
    for (uint8_t box = 0; box < out->count; box++) {
        for (size_t line = 0; line < 2; line++) {
            MUST(TramText_Read(
                reader, 36, &out->allocator, &out->boxes[box].lines[line]));
            Reader_Skip(reader, 4);
        }
    }
    return Reader_Result(reader);
}

static RESULT M_Load(
    VFS *const files, DIALOGUE_ASSETS *const out, const uint8_t stage)
{
    char path[] = "stage0.txt";
    path[5] += stage;
    READER reader;
    MUST(Vfs_Open(files, path, VFS_IN_MEMORY, &reader));
    const RESULT boxes = M_Boxes(&reader, out);
    Reader_Close(&reader);
    MUST(boxes);
    uint8_t blank[36];
    for (size_t i = 0; i < sizeof(blank); i += 2) {
        blank[i] = 0x81;
        blank[i + 1] = 0x40;
    }
    Reader_OpenBuffer(&reader, blank, sizeof(blank), nullptr);
    const RESULT text =
        TramText_Read(&reader, sizeof(blank), &out->allocator, &out->blank);
    Reader_Close(&reader);
    MUST(text);
    MUST(Vfs_Open(files, "miko_k.mpn", VFS_IN_MEMORY, &reader));
    const RESULT faces = Mpn_Load(&reader, &out->allocator, &out->faces);
    Reader_Close(&reader);
    MUST(faces);
    FAIL_IF(out->faces.count < 191, "Missing dialogue portrait tiles");
    return OK;
}

static void M_FreeText(
    const ALLOCATOR *const allocator, const TRAM_TEXT *const text)
{
    if (text->text != nullptr) {
        Allocator_Free(allocator, (void *)text->text, strlen(text->text) + 1);
    }
    Allocator_Free(
        allocator, (void *)text->characters,
        (size_t)text->count * sizeof(*text->characters));
}

RESULT DialogueAssets_Load(
    VFS *const files, const uint8_t stage, const ALLOCATOR *const allocator,
    DIALOGUE_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    FAIL_IF(stage > 5, "Invalid dialogue stage");
    out->allocator = *allocator;
    const RESULT result = M_Load(files, out, stage);
    if (!IS_OK(result)) {
        DialogueAssets_Free(out);
    }
    return result;
}

void DialogueAssets_Free(DIALOGUE_ASSETS *const assets)
{
    for (uint8_t box = 0; box < assets->count; box++) {
        for (size_t line = 0; line < 2; line++) {
            M_FreeText(&assets->allocator, &assets->boxes[box].lines[line]);
        }
    }
    Allocator_Free(
        &assets->allocator, assets->boxes,
        assets->count * sizeof(*assets->boxes));
    M_FreeText(&assets->allocator, &assets->blank);
    Allocator_Free(
        &assets->allocator, assets->faces.pixels,
        (size_t)assets->faces.count * 256);
    memset(assets, 0, sizeof(*assets));
}

// MAIN.EXE's dialogue tables (DGROUP), baked in verbatim: box counts and
// per-script portrait IDs (0x1150), then the stage-specific face lists
// (0x1264-0x12d5) that aren't indexed by the generic counts/faces table.
// stage4_before/after/no_continue store 16-bit portrait IDs in the exe;
// every value is <256, so they're kept as bytes like the others.
static const uint8_t m_Counts[12] = { 16, 8, 22, 11, 8, 3, 16, 4, 0, 0, 22, 6 };
static const uint8_t m_Faces[12][22] = {
    { 0, 96, 51, 96, 9, 96, 0, 96, 99, 99, 48, 12, 99, 99, 6, 99 },
    { 99, 99, 57, 96, 3, 99, 9, 96 },
    { 102, 102, 57, 60,  102, 102, 60,  9, 150, 54, 102,
      102, 96,  54, 102, 60,  9,   102, 9, 150, 57, 150 },
    { 9, 54, 57, 9, 96, 150, 57, 96, 54, 96, 12 },
    { 6, 96, 96, 12, 96, 51, 96 },
    { 12, 96, 12 },
    { 105, 6, 96, 12, 6, 6, 105, 105, 12, 96, 6, 6, 105, 105, 105, 105 },
    { 147, 0, 96 },
    { 0 },
    { 0 },
    { 153, 0,   153, 3,   153, 6,  153, 153, 153, 57, 153,
      57,  153, 57,  153, 153, 57, 153, 12,  96,  12, 153 },
    { 9, 156, 9, 96, 96, 12 },
};
static const uint8_t m_Stage4Intro[8] = { 57, 96, 96, 57, 96, 57, 96, 57 };
static const uint8_t m_Stage4Marisa[11] = { 57, 144, 6,  144, 144, 144,
                                            6,  144, 96, 0,   96 };
static const uint8_t m_Stage4Before[3] = { 96, 54, 144 };
static const uint8_t m_Stage4After[5] = { 144, 12, 96, 57, 144 };
static const uint8_t m_Stage4NoContinue[5] = { 96, 96, 57, 96, 57 };
static const uint8_t m_Stage5Continued[22] = {
    105, 57, 105, 6,  6,   105, 105, 6, 105, 6,   6,
    96,  6,  105, 57, 105, 0,   96,  6, 6,   105, 105,
};
static const uint8_t m_Stage5NoContinue[17] = {
    105, 0, 105, 6, 105, 105, 105, 105, 105, 6, 105, 105, 105, 6, 6, 105, 105,
};
static const uint8_t m_Stage5FormContinued[5] = { 105, 105, 6, 105, 57 };
static const uint8_t m_Stage5FormNoContinue[4] = { 108, 6, 108, 12 };

// The 10 Shift-JIS full-width digit glyphs MAIN.EXE's numeral selector
// pointer table (0x1278) leads to (offsets 0x1306-0x1322), 2 bytes each.
static const uint8_t m_DigitCodes[10][2] = {
    { 0x82, 0x4f }, { 0x82, 0x50 }, { 0x82, 0x51 }, { 0x82, 0x52 },
    { 0x82, 0x53 }, { 0x82, 0x54 }, { 0x82, 0x55 }, { 0x82, 0x56 },
    { 0x82, 0x57 }, { 0x82, 0x58 },
};

static RESULT M_Digit(const uint8_t *const bytes, TRAM_CHARACTER *const digit)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, 2, nullptr);
    TRAM_TEXT text;
    const RESULT result = TramText_Read(&reader, 2, &g_HeapAllocator, &text);
    Reader_Close(&reader);
    MUST(result);
    const bool valid = text.count == 1 && text.characters[0].wide;
    if (valid) {
        *digit = text.characters[0];
    }
    Allocator_Free(&g_HeapAllocator, (void *)text.text, strlen(text.text) + 1);
    Allocator_Free(
        &g_HeapAllocator, (void *)text.characters,
        (size_t)text.count * sizeof(*text.characters));
    FAIL_IF(!valid, "Invalid dialogue numeral");
    return OK;
}

static RESULT M_LoadScripts(DIALOGUE_SCRIPTS *const out)
{
    memcpy(out->counts, m_Counts, sizeof(out->counts));
    memcpy(out->faces, m_Faces, sizeof(out->faces));
    memcpy(out->stage4_intro, m_Stage4Intro, sizeof(out->stage4_intro));
    memcpy(out->stage4_marisa, m_Stage4Marisa, sizeof(out->stage4_marisa));
    memcpy(out->stage4_before, m_Stage4Before, sizeof(out->stage4_before));
    memcpy(out->stage4_after, m_Stage4After, sizeof(out->stage4_after));
    memcpy(
        out->stage4_no_continue, m_Stage4NoContinue,
        sizeof(out->stage4_no_continue));
    memcpy(
        out->stage5_continued, m_Stage5Continued,
        sizeof(out->stage5_continued));
    memcpy(
        out->stage5_no_continue, m_Stage5NoContinue,
        sizeof(out->stage5_no_continue));
    memcpy(
        out->stage5_form_continued, m_Stage5FormContinued,
        sizeof(out->stage5_form_continued));
    memcpy(
        out->stage5_form_no_continue, m_Stage5FormNoContinue,
        sizeof(out->stage5_form_no_continue));
    for (size_t i = 0; i < ARRAY_SIZE(out->counts); i++) {
        FAIL_IF(out->counts[i] > 22, "Invalid dialogue box count");
    }
    for (size_t i = 0; i < ARRAY_SIZE(m_DigitCodes); i++) {
        MUST(M_Digit(m_DigitCodes[i], &out->digits[i]));
    }
    return OK;
}

RESULT DialogueScripts_Load(DIALOGUE_SCRIPTS *const out)
{
    memset(out, 0, sizeof(*out));
    const RESULT result = M_LoadScripts(out);
    if (!IS_OK(result)) {
        memset(out, 0, sizeof(*out));
    }
    return result;
}
