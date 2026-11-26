#include "pc98/pmd/driver.h"

#include "pc98/pmd/data.h"

#include <string.h>

static void M_Require(const RESULT result)
{
    if (!IS_OK(result)) {
        LOG_FATAL("PMD: %s", result.message);
    }
}

static RESULT M_ReleaseEffect(PMD *const clock)
{
    clock->fm.effect_active = false;
    if (clock->sequence.playing) {
        MUST(MusicFm_Restore(
            &clock->fm, clock->sequence.music, clock->sequence.work,
            MUSIC_WORK_SIZE));
        if (clock->board == PMD_BOARD_26) {
            PmdChip_Write(&clock->chip, 0x27, 0x0f);
        }
    }
    return OK;
}

static void M_TimerA(PMD *const clock)
{
    clock->timer_a++;
    PMD_FADE *const fade = &clock->control.fade;
    if ((clock->timer_a & 7) == 0 && fade->speed != 0) {
        const unsigned sum = (unsigned)fade->volume + fade->speed;
        if (fade->speed < 128) {
            if (sum >= 256) {
                fade->volume = 255;
                fade->speed = 0;
                if (fade->stop_on_end) {
                    fade->pending_stop = true;
                }
            } else {
                fade->volume = (uint8_t)sum;
            }
        } else if (sum < 256) {
            fade->volume = fade->speed = 0;
        } else {
            fade->volume = (uint8_t)sum;
        }
    }
    RESULT result = OK;
    if (!clock->failed && clock->drum.priority != 0) {
        result = SsgEffects_Step(&clock->drum);
        clock->ssg.effect_active = clock->drum.priority != 0;
        clock->ssg.effect_id = clock->drum.id;
    }
    if (IS_OK(result) && !clock->failed && clock->effect.playing) {
        clock->effect.control.random_seed = clock->control.random_seed;
        clock->effect.control.tied = clock->control.tied;
        result = PmdEffect_Step(
            &clock->effect, (uint8_t)(clock->timer_a - clock->last_timer_a),
            fade->speed != 0);
        clock->control.random_seed = clock->effect.control.random_seed;
        clock->control.tied = clock->effect.control.tied;
        if (IS_OK(result) && !clock->effect.playing) {
            result = M_ReleaseEffect(clock);
        }
    }
    if (!IS_OK(result)) {
        clock->error = result;
        clock->failed = true;
        PmdEffect_Stop(&clock->effect);
        MusicClock_StopSsgEffect(clock);
        clock->fm.effect_active = false;
        MusicSeq_Stop(clock);
    }
}

static void M_SetEffectMask(PMD *const clock, const uint8_t part)
{
    if (clock->dispatching && clock->dispatch_part == part
        && clock->sequence.parts[part].mask == 0) {
        // Inline effects do not switch their issuing part's current parser
        // branch. Apply its mask after dispatch, or at an explicit mask change.
        clock->deferred_masks |= (uint16_t)(1u << part);
    } else {
        MusicSeq_SetMask(
            &clock->sequence, part,
            (clock->sequence.parts[part].mask & 15) | 2);
    }
}

static void M_Stop(PMD *const clock, const bool external)
{
    if (external) {
        clock->control.fade.inline_requested = false;
    }
    if (clock->sequence.playing) {
        MusicSeq_Stop(clock);
    } else {
        MusicClock_Silence(clock);
    }
}

static void M_TimerB(PMD *const clock)
{
    if (clock->failed) {
        return;
    }
    if (clock->control.fade.pending_stop) {
        M_Stop(clock, false);
    }
    const bool playing = clock->sequence.playing;
    clock->timer_delta = (uint8_t)(clock->timer_a - clock->last_timer_a);
    const RESULT result = MusicSeq_Step(clock);
    if (playing) {
        clock->last_timer_a = clock->timer_a;
    }
    if (!IS_OK(result)) {
        if (IS_OK(clock->error)) {
            clock->error = result;
        } else {
            Result_Ignore(result);
        }
        clock->failed = true;
        PmdEffect_Stop(&clock->effect);
        if (clock->drum.priority != 0) {
            MusicClock_StopSsgEffect(clock);
        }
        clock->fm.effect_active = false;
        return;
    }
    if (clock->failed) {
        clock->sequence.failed = true;
        PmdEffect_Stop(&clock->effect);
        if (clock->drum.priority != 0) {
            MusicClock_StopSsgEffect(clock);
        }
        clock->fm.effect_active = false;
        MusicSeq_Stop(clock);
        return;
    }
    if (clock->timer_b != clock->sequence.timer_b) {
        clock->timer_b = clock->sequence.timer_b;
        PmdChip_Write(&clock->chip, 0x26, clock->timer_b);
    }
}

static void M_Timer(
    void *const context, OPNA *const chip, const unsigned timer,
    const uint64_t time)
{
    PMD *const clock = context;
    if (timer == 1) {
        PmdChip_ReloadTimerB(&clock->chip, time);
    }
    if ((Opna_ReadPort(chip, 0) & (1u << timer)) == 0) {
        return;
    }
    // A shared IRQ acknowledges coincident timers once, then handles B first.
    if (timer == 0 && clock->chip.next_b == time) {
        clock->pending_a = true;
        return;
    }
    PmdChip_Write(&clock->chip, 0x27, 0x3f);
    if (timer == 0) {
        M_TimerA(clock);
    } else {
        M_TimerB(clock);
        if (clock->pending_a) {
            clock->pending_a = false;
            M_TimerA(clock);
        }
    }
}

void MusicClock_StartPorts(PMD *const clock)
{
    if (clock->board == PMD_BOARD_86) {
        PmdRhythm_Reset(&clock->rhythm);
    }
    PmdChip_Write(&clock->chip, 0x29, 0x83);
    if (!clock->ssg.effect_active) {
        PmdChip_Write(&clock->chip, 6, 0);
    }
    for (uint16_t reg = 0xb4; reg <= 0xb6; reg++) {
        PmdChip_Write(&clock->chip, reg, 0xc0);
        if (clock->board == PMD_BOARD_86
            && (reg != 0xb6 || !clock->fm.effect_active)) {
            PmdChip_Write(&clock->chip, reg + 256, 0xc0);
        }
    }
    PmdChip_Write(&clock->chip, 0x22, 0);
    if (clock->board == PMD_BOARD_26) {
        PmdChip_Write(&clock->chip, 0x110, 0x80);
        PmdChip_Write(&clock->chip, 0x110, 0x18);
    }
    if (clock->timer_b != clock->sequence.timer_b) {
        clock->timer_b = clock->sequence.timer_b;
        PmdChip_Write(&clock->chip, 0x26, clock->timer_b);
    }
    PmdChip_Write(&clock->chip, 0x25, 0);
    PmdChip_Write(&clock->chip, 0x24, 0);
    PmdChip_Write(&clock->chip, 0x27, 0x3f);
}

void MusicClock_Silence(PMD *const clock)
{
    clock->dispatching = false;
    clock->deferred_masks = 0;
    clock->control.fade.volume = 255;
    clock->control.fade.speed = 0;
    clock->control.fade.pending_stop = false;
    MusicFm_Stop(&clock->fm);
    MusicSsg_Stop(&clock->ssg);
    if (clock->board == PMD_BOARD_86) {
        PmdChip_Write(&clock->chip, 0x110, 0x80);
        PmdChip_Write(&clock->chip, 0x110, 0x18);
    }
}

RESULT MusicClock_PlayEffect(PMD *const clock, const uint8_t id)
{
    FAIL_IF(
        clock->failed || id >= PMD_EFFECT_COUNT, "Invalid PMD effect request");
    MUST(MusicClock_StopEffect(clock));
    for (uint8_t part = clock->board == PMD_BOARD_86 ? 5 : 2; part <= 5;
         part++) {
        M_SetEffectMask(clock, part);
    }
    clock->fm.effect_active = true;
    return PmdEffect_Play(&clock->effect, id);
}

RESULT MusicClock_StopEffect(PMD *const clock)
{
    if (!clock->effect.playing) {
        return OK;
    }
    PmdEffect_Stop(&clock->effect);
    return M_ReleaseEffect(clock);
}

RESULT MusicClock_PlaySsgEffect(PMD *const clock, const uint8_t id)
{
    FAIL_IF(
        clock->failed || id >= clock->drum.bank->count,
        "Invalid PMD SSG effect request");
    if (clock->drum.priority <= clock->drum.bank->priorities[id]) {
        M_SetEffectMask(clock, 8);
    }
    MUST(SsgEffects_Play(&clock->drum, id));
    clock->ssg.effect_active = clock->drum.priority != 0;
    clock->ssg.effect_id = clock->drum.id;
    return OK;
}

void MusicClock_StopSsgEffect(PMD *const clock)
{
    SsgEffects_Stop(&clock->drum);
    clock->ssg.effect_active = false;
    clock->ssg.effect_id = clock->drum.id;
}

RESULT Pmd_Init(
    const PMD_EFFECTS *const effects, const PMD_BOARD board,
    const uint8_t initial_timer_b, const ALLOCATOR *const allocator,
    PMD **const out)
{
    *out = nullptr;
    FAIL_IF(effects == nullptr, "Missing PMD effects");
    FAIL_IF(
        board != PMD_BOARD_26 && board != PMD_BOARD_86, "Invalid PMD board");
    PMD *const pmd = Allocator_AllocZ(allocator, sizeof(*pmd));
    pmd->board = board;
    MusicSeq_Init(pmd, allocator);
    PmdRhythm_Init(&pmd->rhythm, &pmd->chip, &pmd->control);
    pmd->control.fade.stop_on_end = true;
    PmdChip_Init(&pmd->chip, allocator, M_Timer, pmd);
    MusicFm_Init(&pmd->fm, &pmd->control, &pmd->chip, board);
    MusicSsg_Init(&pmd->ssg, &pmd->control, &pmd->chip);
    if (board == PMD_BOARD_86) {
        PmdChip_Write(&pmd->chip, 0x29, 0x83);
    }
    PmdChip_Write(&pmd->chip, 7, 0xbf);
    pmd->timer_b = pmd->sequence.timer_b;
    PmdChip_Write(&pmd->chip, 0x25, 0);
    PmdChip_Write(&pmd->chip, 0x24, 0);
    PmdChip_Write(&pmd->chip, 0x26, initial_timer_b);
    PmdChip_Write(&pmd->chip, 0x27, 0x3f);
    PmdChip_Write(&pmd->chip, 0x26, pmd->timer_b);
    MUST(SsgEffects_LoadTable(&pmd->drum_bank));
    SsgEffects_Init(&pmd->drum, &pmd->drum_bank, &pmd->chip);
    MUST(PmdEffect_Init(
        &pmd->effect, effects, allocator, &pmd->chip,
        board == PMD_BOARD_86 ? 5 : 2));
    if (board == PMD_BOARD_86) {
        MusicClock_Silence(pmd);
    }
    *out = pmd;
    return OK;
}

void Pmd_Load(PMD *const pmd, const PMD_SONG *const song)
{
    M_Require(MusicSeq_Load(pmd, song));
    Result_Ignore(pmd->error);
    pmd->error = OK;
    pmd->failed = false;
}

void Pmd_Start(PMD *const pmd)
{
    if (pmd->failed) {
        LOG_FATAL("PMD: PMD clock requires a song reload");
    }
    if (pmd->sequence.music == nullptr) {
        LOG_FATAL("PMD: Missing PMD song");
    }
    pmd->sequence.timer_b = 200;
    pmd->sequence.tempo = 78;
    M_Require(MusicSeq_Start(pmd));
}

void Pmd_Stop(PMD *const pmd)
{
    M_Stop(pmd, true);
}

void Pmd_Fade(PMD *const pmd, const int8_t speed)
{
    pmd->control.fade.speed = (uint8_t)speed;
}

void Pmd_Effect(PMD *const pmd, const uint8_t effect)
{
    M_Require(MusicClock_PlayEffect(pmd, effect));
}

uint16_t Pmd_Measure(const PMD *const pmd)
{
    return pmd->sequence.measure;
}

void Pmd_Render(PMD *const pmd, OPNA_FRAME *const output, const size_t frames)
{
    if (pmd->failed) {
        LOG_FATAL("PMD: PMD clock requires a song reload");
    }
    Opna_Render(pmd->chip.opna, output, frames);
    const RESULT result = pmd->error;
    pmd->error = OK;
    M_Require(result);
}
