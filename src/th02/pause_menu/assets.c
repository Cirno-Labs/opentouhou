#include "th02/pause_menu/assets.h"

#include "core/base/assert.h"

#include <string.h>

// MAIN.EXE's five pause labels (DGROUP), Shift-JIS, in PAUSE_MENU_DRAW_KIND
// order (Resume, Quit, Prompt, No, Yes), baked in without their zero
// terminator.
static const uint8_t m_Resume[] = { 0x83, 0x51, 0x81, 0x5b, 0x83,
                                    0x80, 0x8d, 0xc4, 0x8a, 0x4a };
static const uint8_t m_Quit[] = { 0x83, 0x51, 0x81, 0x5b, 0x83,
                                  0x80, 0x8f, 0x49, 0x97, 0xb9 };
static const uint8_t m_Prompt[] = {
    0x82, 0xd9, 0x82, 0xf1, 0x82, 0xc6, 0x82, 0xc9, 0x8f, 0x49, 0x97,
    0xb9, 0x82, 0xb5, 0x82, 0xbf, 0x82, 0xe1, 0x82, 0xa4, 0x82, 0xcc,
};
static const uint8_t m_No[] = {
    0x82, 0xa4, 0x82, 0xbb, 0x82, 0xc5, 0x82, 0xb7, 0x81, 0x42, 0x82,
    0xb7, 0x82, 0xdd, 0x82, 0xdc, 0x82, 0xb9, 0x82, 0xf1, 0x81, 0x42,
};
static const uint8_t m_Yes[] = {
    0x81, 0x40, 0x81, 0x40, 0x82, 0xcd, 0x82, 0xa2, 0x81, 0x41, 0x82,
    0xe2, 0x82, 0xdf, 0x82, 0xdc, 0x82, 0xb7, 0x81, 0x42, 0x81, 0x40,
};

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

static RESULT M_Load(PAUSE_MENU_ASSETS *const assets)
{
    MUST(M_Text(
        m_Resume, sizeof(m_Resume), &assets->allocator, &assets->labels[0]));
    MUST(
        M_Text(m_Quit, sizeof(m_Quit), &assets->allocator, &assets->labels[1]));
    MUST(M_Text(
        m_Prompt, sizeof(m_Prompt), &assets->allocator, &assets->labels[2]));
    MUST(M_Text(m_No, sizeof(m_No), &assets->allocator, &assets->labels[3]));
    MUST(M_Text(m_Yes, sizeof(m_Yes), &assets->allocator, &assets->labels[4]));
    return OK;
}

RESULT PauseMenuAssets_Load(
    const ALLOCATOR *const allocator, PAUSE_MENU_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    out->allocator = *allocator;
    const RESULT result = M_Load(out);
    if (!IS_OK(result)) {
        PauseMenuAssets_Free(out);
    }
    return result;
}

void PauseMenuAssets_Free(PAUSE_MENU_ASSETS *const assets)
{
    for (size_t i = 0; i < ARRAY_SIZE(assets->labels); i++) {
        const TRAM_TEXT *const text = &assets->labels[i];
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
