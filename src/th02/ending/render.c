#include "th02/ending/render.h"

#include "th02/ending/images.h"
#include "th02/ending/text.h"
#include "th02/grcg.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

static void M_RotatedRectangle(const int16_t radius, uint8_t angle)
{
    MATH16_VECTOR corners[4];
    for (size_t i = 0; i < ARRAY_SIZE(corners); i++) {
        corners[i].x = Math16_PolarX(192, radius, angle);
        corners[i].y = Math16_PolarY(200, radius, angle);
        angle = (uint8_t)(angle + (i % 2 == 0 ? 0x2f : 0x51));
    }
    for (size_t i = 0; i < ARRAY_SIZE(corners); i++) {
        const MATH16_VECTOR next = corners[(i + 1) % ARRAY_SIZE(corners)];
        Draw_Line(g_Clip, corners[i].x, corners[i].y, next.x, next.y);
    }
}

static void M_Score(
    const GAIJI *const gaiji, const int32_t score, const int16_t top)
{
    int32_t divisor = 10000000;
    bool significant = false;
    for (int16_t i = 0; i < 8; i++) {
        const int32_t digit = score / divisor % 10;
        divisor /= 10;
        significant |= digit != 0;
        if (significant) {
            Draw_Gaiji(
                gaiji, 192 + i * 16, top, (uint8_t)(0xa0 + digit), 15, false);
        }
    }
}

static uint8_t M_SkillTitle(const int16_t skill)
{
    if (skill == 100) {
        return 0;
    }
    if (skill >= 80) {
        return (uint8_t)(10 - skill / 10);
    }
    if (skill == 77) {
        return 3;
    }
    if (skill >= 70) {
        return 4;
    }
    return (uint8_t)(11 - skill / 10);
}

static void M_RevealCell(
    const uint16_t rows[16], const int16_t left, const int16_t top)
{
    for (size_t y = 0; y < 16; y++) {
        const size_t offset = ((size_t)top + y) * VIDEO_WIDTH + left;
        for (size_t x = 0; x < 16; x++) {
            if ((rows[y] & (0x8000u >> x)) != 0) {
                g_Video.pages[0][offset + x] = g_Video.pages[1][offset + x];
            }
        }
    }
    Video_AccessPage(0);
    Grcg_SetColor(0);
    Grcg_Off();
}

void EndingRender_BeginRotation(
    ENDING_ROTATION *const rotation, const uint8_t speed, const uint8_t angle)
{
    memset(rotation, 0, sizeof(*rotation));
    rotation->speed = speed;
    for (size_t i = 0; i < ARRAY_SIZE(rotation->radii); i++) {
        rotation->radii[i] = 256;
        rotation->angles[i] = angle;
    }
}

bool EndingRender_StepRotation(ENDING_ROTATION *const rotation)
{
    if (rotation->step > 20) {
        return true;
    }
    Grcg_SetColor(0);
    if (rotation->step == 0 || rotation->step == 20) {
        Draw_BoxFill(g_Clip, 0, 0, 384, 399);
    }
    if (rotation->step == 20) {
        Grcg_SetColor(15);
        Draw_Line(g_Clip, 356, 96, 356, 304);
        Draw_Line(g_Clip, 356, 304, 28, 304);
        Draw_Line(g_Clip, 28, 304, 28, 96);
        Draw_Line(g_Clip, 28, 96, 356, 96);
        Grcg_Off();
        Palette_SetTone(200);
        rotation->step++;
        return true;
    }
    M_RotatedRectangle(rotation->radii[0], rotation->angles[0]);
    rotation->radii[0] = rotation->radii[1];
    rotation->angles[0] = rotation->angles[1];
    Grcg_SetColor(4);
    for (size_t i = 1; i < 4; i++) {
        M_RotatedRectangle(rotation->radii[i], rotation->angles[i]);
        rotation->radii[i] = rotation->radii[i + 1];
        rotation->angles[i] = rotation->angles[i + 1];
    }
    Grcg_SetColor(15);
    if (rotation->step < 16) {
        rotation->radii[4] -= 4;
        rotation->angles[4] = (uint8_t)(rotation->angles[4] + rotation->speed);
    }
    M_RotatedRectangle(rotation->radii[4], rotation->angles[4]);
    rotation->step++;
    return false;
}

RESULT EndingRender_LoadRevealMasks(
    VFS *const files, ENDING_REVEAL_MASKS *const out)
{
    memset(out, 0, sizeof(*out));
    uint8_t *verdict;
    size_t verdict_size;
    MUST(SpriteSheet_Load(
        files, "verdict.bmp", 16, 16, &g_HeapAllocator, &verdict,
        &verdict_size));
    RESULT result = OK;
    if (verdict_size != ENDING_REVEAL_STEPS * 16 * 2) {
        result = FAIL("Invalid verdict.bmp size");
    } else {
        for (size_t step = 0; step < ENDING_REVEAL_STEPS; step++) {
            for (size_t y = 0; y < 16; y++) {
                const uint16_t left = verdict[(step * 16 + y) * 2];
                const uint16_t right = verdict[(step * 16 + y) * 2 + 1];
                out->rows[step][y] = (uint16_t)(left << 8) | right;
            }
        }
    }
    Mem_Free(verdict);
    return result;
}

void EndingRender_BeginReveal(
    ENDING_REVEAL *const reveal, const ENDING_REVEAL_MASKS *const masks,
    const int16_t left, const int16_t top, const uint16_t columns)
{
    ASSERT(left >= 0 && left % 8 == 0);
    ASSERT((int32_t)left + (int32_t)columns * 16 <= VIDEO_WIDTH);
    ASSERT(top >= 0 && top <= VIDEO_HEIGHT - 16);
    memset(reveal, 0, sizeof(*reveal));
    reveal->masks = masks;
    reveal->left = left;
    reveal->top = top;
    reveal->columns = columns;
}

bool EndingRender_StepReveal(ENDING_REVEAL *const reveal)
{
    if (reveal->step == ENDING_REVEAL_STEPS) {
        return true;
    }
    for (uint16_t column = 0; column < reveal->columns; column++) {
        M_RevealCell(
            reveal->masks->rows[reveal->step],
            (int16_t)(reveal->left + column * 16), reveal->top);
    }
    reveal->step++;
    return false;
}

void EndingRender_Stage(
    const ENDING_IMAGES *const images, const ENDING_IMAGE image,
    const bool apply_palette)
{
    Video_AccessPage(1);
    if (apply_palette) {
        EndingImages_ApplyPalette(images, image);
    }
    EndingImages_Draw(images, image);
}

void EndingRender_Copy(
    const int16_t left, const int16_t top, const uint8_t quarter,
    const int16_t source_top, const int16_t rows)
{
    ASSERT(quarter < 4 && left >= 0 && left % 16 == 0 && left <= 320);
    ASSERT(top >= 0 && rows >= 0 && (int32_t)top + rows <= VIDEO_HEIGHT);
    ASSERT(source_top >= 0 && (int32_t)source_top + rows <= 200);
    const size_t source =
        (size_t)((quarter / 2) * 200 + source_top) * VIDEO_WIDTH
        + (quarter % 2) * 320;
    const size_t destination = (size_t)top * VIDEO_WIDTH + left;
    for (size_t row = 0; row < (size_t)rows; row++) {
        memcpy(
            g_Video.pages[0] + destination + row * VIDEO_WIDTH,
            g_Video.pages[1] + source + row * VIDEO_WIDTH, 320);
    }
    if (rows != 0) {
        const uint16_t last_word =
            (uint16_t)((source + (size_t)(rows - 1) * VIDEO_WIDTH) / 8 + 38);
        for (uint8_t plane = 0; plane < 4; plane++) {
            Grcg_LoadTile(plane, Video_ReadPlaneByte(1, plane, last_word));
        }
        Video_AccessPage(0);
    }
    Grcg_Off();
}

void EndingRender_Credits(
    const ENDING_LABELS *const labels, const ENDING_CREDIT section)
{
    ASSERT(
        section >= ENDING_CREDIT_PROGRAM && section <= ENDING_CREDIT_TESTERS);
    static const struct {
        ENDING_LABEL label;
        int16_t x;
        int16_t y;
    } lines[] = {
        { ENDING_LABEL_PROGRAM, 416, 192 },
        { ENDING_LABEL_GRAPHICS_1, 416, 192 },
        { ENDING_LABEL_GRAPHICS_2, 416, 224 },
        { ENDING_LABEL_GRAPHICS_3, 416, 240 },
        { ENDING_LABEL_MUSIC, 432, 192 },
        { ENDING_LABEL_TESTERS_1, 416, 192 },
        { ENDING_LABEL_TESTERS_2, 416, 224 },
        { ENDING_LABEL_TESTERS_3, 416, 248 },
        { ENDING_LABEL_TESTERS_4, 416, 272 },
        { ENDING_LABEL_TESTERS_5, 416, 296 },
    };
    static const uint8_t first[] = { 0, 1, 4, 5, 10 };
    Grcg_SetColor(0);
    Draw_BoxFill(g_Clip, 416, 192, 639, 271);
    Grcg_Off();
    for (size_t i = first[section]; i < first[section + 1]; i++) {
        const DRAW_TEXT *const text = &labels->text[lines[i].label];
        EndingText_Put(text, UINT16_MAX, lines[i].x, lines[i].y, 0x2f);
    }
}

void EndingRender_VerdictRow(
    const ENDING_LABELS *const labels, const ENDING_TEXT_SCRIPT *const titles,
    const GAIJI *const gaiji, const TH02_SESSION *const session,
    const uint8_t row, const uint16_t style)
{
    ASSERT(row <= 8 && row != 7);
    ASSERT(session->rank >= 0 && session->rank < 4);
    ASSERT(session->continues_used <= 3);
    ASSERT(session->start_lives <= 4 && session->start_bombs <= 5);
    ASSERT(titles->count >= 12);
    const ENDING_LABEL label =
        row == 8 ? ENDING_LABEL_COPYRIGHT : ENDING_LABEL_THANK_YOU + row;
    const int16_t top = 96 + row * 32;
    EndingText_Put(
        &labels->text[label], UINT16_MAX, row == 0 ? 24 : 64, top, style);
    switch (row) {
    case 1:
        M_Score(gaiji, session->score, top);
        break;

    case 2:
        Draw_Gaiji(
            gaiji, 256, top, (uint8_t)(0xa0 + session->continues_used), 15,
            false);
        break;

    case 3:
        Draw_GaijiString(gaiji, 208, top, 16, labels->ranks[session->rank], 15);
        break;

    case 4:
        Draw_Gaiji(
            gaiji, 256, top, (uint8_t)(0xa1 + session->start_lives), 15, false);
        break;

    case 5:
        Draw_Gaiji(
            gaiji, 256, top, (uint8_t)(0xa0 + session->start_bombs), 15, false);
        break;

    case 6: {
        int16_t skill = session->skill;
        if (skill < 0) {
            skill = 0;
        } else if (skill > 100) {
            skill = 100;
        }
        uint8_t digits[4] = {};
        int16_t value = skill;
        int16_t divisor = 100;
        bool significant = false;
        for (size_t i = 0; i < 3; i++) {
            const int16_t digit = value / divisor;
            significant |= digit != 0;
            digits[i] = significant || i == 2 ? (uint8_t)(0xa0 + digit) : 0xcf;
            value %= divisor;
            divisor /= 10;
        }
        Draw_GaijiString(gaiji, 192, top, 16, digits, 15);
        EndingText_Put(
            &titles->lines[M_SkillTitle(skill)], UINT16_MAX, 240, top, style);
        break;
    }
    }
}
