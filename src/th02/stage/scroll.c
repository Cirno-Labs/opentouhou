#include "th02/stage/scroll.h"
#include "th02/background.h"
#include "th02/grcg.h"
#include "th02/stage/private.h"
#include "th02/video.h"

#include "core/base/assert.h"

void StageScroll_InitPages(void)
{
    p_Stage->scroll.page_lines[0] = p_Stage->scroll.page_lines[1] = 0;
}

void StageScroll_Begin(void)
{
    ASSERT(p_Stage->scroll.back < 2);
    p_Stage->scroll.previous = p_Stage->scroll.line;
    p_Stage->scroll.line = p_Stage->scroll.page_lines[p_Stage->scroll.back];
}

void StageScroll_Advance(void)
{
    ASSERT(p_Stage->scroll.back < 2);
    p_Stage->scroll.moved = 0;
    p_Stage->scroll.line = p_Stage->scroll.previous;
    if (p_Stage->scroll.done == 0) {
        ASSERT(p_Stage->scroll.interval != 0);
        if (p_Stage->scroll.cycle % p_Stage->scroll.interval == 0) {
            p_Stage->scroll.line = (int16_t)(uint16_t)(p_Stage->scroll.line
                                                       - p_Stage->scroll.speed);
            if (p_Stage->scroll.line < 0) {
                p_Stage->scroll.line =
                    (int16_t)(uint16_t)(p_Stage->scroll.line + 400);
            }
            p_Stage->scroll.moved = p_Stage->scroll.speed;
        }
    }
    p_Stage->scroll.page_lines[p_Stage->scroll.back] = p_Stage->scroll.line;
}

void StageScroll_Finish(void)
{
    if (p_Stage->scroll.moved != 0) {
        p_Stage->scroll.address =
            (uint16_t)((uint16_t)p_Stage->scroll.line * 40u);
        if (((uint16_t)p_Stage->scroll.line & 7u) == 0) {
            p_Stage->scroll.rows++;
            p_Stage->scroll.row_changed = 1;
        }
        p_Stage->scroll.done =
            Background_Scroll(StageScroll_Pages(), p_Stage->scroll.speed);
        Grcg_Off();
        if (p_Stage->scroll.done != 0) {
            p_Stage->scroll.line =
                (int16_t)(uint16_t)(p_Stage->scroll.line + 1);
            return;
        }
        Video_SetSplit(400 - p_Stage->scroll.line, p_Stage->scroll.address, 0);
    }
    p_Stage->scroll.cycle++;
}

void StageScroll_Swap(void)
{
    ASSERT(p_Stage->scroll.front < 2 && p_Stage->scroll.back < 2);
    p_Stage->scroll.front = p_Stage->scroll.back;
    Video_ShowPage(p_Stage->scroll.front);
    p_Stage->scroll.back ^= 1;
    Video_AccessPage(p_Stage->scroll.back);
}

PLAYFIELD_PAGES StageScroll_Pages(void)
{
    return (PLAYFIELD_PAGES) {
        .back = g_Stage->scroll.back,
        .front = g_Stage->scroll.front,
        .scroll = g_Stage->scroll.line,
    };
}
