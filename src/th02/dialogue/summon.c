#include "th02/dialogue/summon.h"

#include "th02/clip.h"
#include "th02/grcg.h"

#include <string.h>

static void M_Circles(const int radius, const bool erase)
{
    Grcg_SetColor(erase ? 0 : 10);
    Draw_CircleFill(224, 144, radius);
    Grcg_SetColor(erase ? 0 : 13);
    Draw_Circle(224, 144, (radius % 50) * 4);
    Grcg_SetColor(erase ? 0 : 15);
    Draw_Circle(224, 144, (radius % 30) * 7);
}

void DialogueSummon_Begin(
    DIALOGUE_SUMMON *const summon, const PATTERN_BANK *const patterns,
    void (*const draw)(const PATTERN_BANK *patterns))
{
    memset(summon, 0, sizeof(*summon));
    summon->patterns = patterns;
    summon->draw = draw;
    summon->radius = 150;
    Clip_SetSummonArea();
    SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    M_Circles(summon->radius, false);
    Wait_Delay(&summon->wait, 1);
}

bool DialogueSummon_Update(DIALOGUE_SUMMON *const summon)
{
    if (summon->done) {
        return true;
    }
    if (!Wait_Ready(&summon->wait, nullptr, 0)) {
        return false;
    }
    if (summon->radius != 0) {
        M_Circles(summon->radius, true);
        Palette_SetTone((int16_t)(((150 - summon->radius) / 3) * 2 + 100));
        SoundEffects_Update();
        summon->radius--;
        if (summon->radius != 0) {
            M_Circles(summon->radius, false);
            Wait_Delay(&summon->wait, 1);
            return false;
        }
        Grcg_SetColor(0);
        Draw_BoxFill(
            g_Clip, g_Clip->left, g_Clip->top, g_Clip->right, g_Clip->bottom);
        Grcg_Off();
        if (summon->draw != nullptr) {
            summon->draw(summon->patterns);
        }
    } else {
        summon->fade++;
    }
    Palette_SetTone((int16_t)(200 - summon->fade * 10));
    if (summon->fade == 10) {
        summon->done = true;
        return true;
    }
    Wait_Delay(&summon->wait, 3);
    return false;
}
