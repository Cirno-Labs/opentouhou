#include "th02/stage_bonus/assets.h"

#include "th02/tram.h"

#include <string.h>

static TRAM_TEXT *M_Field(STAGE_BONUS_ASSETS *const assets, const size_t index)
{
    if (index < ARRAY_SIZE(assets->labels)) {
        return &assets->labels[index];
    }
    return index == 10 ? &assets->point
        : index == 11  ? &assets->times
                       : &assets->equals;
}

// MAIN.EXE's 13 bonus/Extra labels (DGROUP), Shift-JIS, baked in verbatim
// without their zero terminator (TramText_Read takes an explicit size).
static const uint8_t m_Labels[][12] = {
    { 0x20, 0x93, 0xef, 0x88, 0xd5, 0x93, 0x78 },
    { 0x83, 0x58, 0x83, 0x65, 0x81, 0x5b, 0x83, 0x57 },
    { 0x83, 0x7b, 0x83, 0x80 },
    { 0x83, 0x7e, 0x83, 0x58 },
    { 0xe8, 0xcb, 0x8c, 0x82, 0x8f, 0x89, 0x8a, 0xfa, 0x90, 0x94 },
    { 0xe8, 0xcb, 0x96, 0xb2, 0x8f, 0x89, 0x8a, 0xfa, 0x90, 0x94 },
    { 0x83, 0x4e, 0x83, 0x8a, 0x83, 0x41 },
    { 0x83, 0x7e, 0x83, 0x58, 0x89, 0xf1, 0x90, 0x94 },
    { 0x83, 0x7b, 0x83, 0x80, 0x89, 0xf1, 0x90, 0x94 },
    { 0x83, 0x4e, 0x83, 0x8a, 0x83, 0x41, 0x83, 0x5e, 0x83, 0x43, 0x83, 0x80 },
};
static const uint8_t m_Point[] = { 0x93, 0x5f };
static const uint8_t m_Times[] = { 0x81, 0x7e };
static const uint8_t m_Equals[] = { 0x81, 0x81 };
static const uint8_t m_LabelSizes[10] = { 7, 8, 4, 4, 10, 10, 6, 8, 8, 12 };

static RESULT M_Text(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, TRAM_TEXT *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = TramText_Read(&reader, size, allocator, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_Load(STAGE_BONUS_ASSETS *const assets)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Labels); i++) {
        MUST(M_Text(
            m_Labels[i], m_LabelSizes[i], &assets->allocator,
            M_Field(assets, i)));
    }
    MUST(M_Text(m_Point, sizeof(m_Point), &assets->allocator, &assets->point));
    MUST(M_Text(m_Times, sizeof(m_Times), &assets->allocator, &assets->times));
    MUST(M_Text(
        m_Equals, sizeof(m_Equals), &assets->allocator, &assets->equals));
    return OK;
}

RESULT StageBonusAssets_Load(
    const ALLOCATOR *const allocator, STAGE_BONUS_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    out->allocator = *allocator;
    const RESULT result = M_Load(out);
    if (!IS_OK(result)) {
        StageBonusAssets_Free(out);
    }
    return result;
}

void StageBonusAssets_Free(STAGE_BONUS_ASSETS *const assets)
{
    for (size_t i = 0; i < 13; i++) {
        const TRAM_TEXT *const text = M_Field(assets, i);
        if (text->text != nullptr) {
            Allocator_Free(
                &assets->allocator, (void *)text->text, strlen(text->text) + 1);
        }
        Allocator_Free(
            &assets->allocator, (void *)text->characters,
            (size_t)text->count * sizeof(*text->characters));
    }
    memset(assets, 0, sizeof(*assets));
}
