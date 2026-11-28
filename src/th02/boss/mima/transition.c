#include "th02/background.h"
#include "th02/boss/mima/assets.h"
#include "th02/boss/mima/private.h"
#include "th02/boss/private.h"
#include "th02/boss/scene.h"
#include "th02/clip.h"
#include "th02/dialogue/sequence.h"
#include "th02/dialogue/summon.h"
#include "th02/draw/shapes.h"
#include "th02/grcg.h"
#include "th02/hud.h"
#include "th02/palette.h"
#include "th02/sound_effects.h"
#include "th02/stage.h"
#include "th02/video.h"

#include "core/base/assert.h"

#include <string.h>

// Mima's own scripted sequences: the stage 5 pre-fight reveal, and the two
// form-change scripts reached through the talk.transition hook.
typedef enum {
    MIMA_SCRIPT_PRE,
    MIMA_SCRIPT_FORM_DEFEAT,
    MIMA_SCRIPT_POST,
} MIMA_SCRIPT;

typedef enum {
    M_BOX,
    M_FLASH,
    M_DELAY,
    M_WHITE_OUT,
} M_OPERATION_KIND;

typedef struct {
    M_OPERATION_KIND kind;
    uint8_t frames;
} M_OPERATION;

static const M_OPERATION m_Post[] = {
    { M_BOX, 0 },    { M_FLASH, 0 },  { M_DELAY, 10 }, { M_BOX, 0 },
    { M_DELAY, 30 }, { M_FLASH, 0 },  { M_DELAY, 20 }, { M_BOX, 0 },
    { M_DELAY, 20 }, { M_BOX, 0 },    { M_FLASH, 0 },  { M_DELAY, 20 },
    { M_FLASH, 0 },  { M_DELAY, 20 }, { M_FLASH, 0 },  { M_WHITE_OUT, 3 },
};

static MIMA_TALK m_Talk;
MIMA_TALK *const p_Talk = &m_Talk;

static void M_Music(
    const BOSS_MUSIC *const music, const BOSS_MUSIC_REQUEST request)
{
    if (music->transition != nullptr) {
        music->transition(music->context, request);
    }
}

static void M_Delay(const uint16_t frames, const MIMA_TALK_PHASE after)
{
    Wait_Delay(&m_Talk.wait, frames);
    m_Talk.after_wait = after;
    m_Talk.phase = MIMA_TALK_DELAY;
}

static void M_Fade(
    const PALETTE_FADE_KIND kind, const int16_t speed,
    const MIMA_TALK_PHASE after)
{
    Wait_Fade(&m_Talk.wait, kind, speed);
    m_Talk.after_wait = after;
    m_Talk.phase = MIMA_TALK_FADE;
}

static void M_Effect(const uint8_t id, const bool force)
{
    if (force) {
        SoundEffects_Force(id);
    } else {
        SoundEffects_Play(id);
        SoundEffects_Update();
    }
}

static void M_Post(void)
{
    ASSERT(m_Talk.operation < ARRAY_SIZE(m_Post));
    const M_OPERATION operation = m_Post[m_Talk.operation];
    if (operation.kind == M_FLASH) {
        M_Fade(PALETTE_WHITE_OUT, 0, MIMA_TALK_FLASH_FORCE);
    } else if (operation.kind == M_DELAY) {
        m_Talk.operation++;
        M_Delay(operation.frames, MIMA_TALK_POST_RUN);
    } else if (operation.kind == M_WHITE_OUT) {
        M_Fade(PALETTE_WHITE_OUT, operation.frames, MIMA_TALK_DONE);
    }
}

static void M_BeginScript(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const uint16_t continues,
    const MIMA_SCRIPT script)
{
    ASSERT(
        m_Talk.runtime.box
        == (script == MIMA_SCRIPT_PRE               ? 0
                : script == MIMA_SCRIPT_FORM_DEFEAT ? 44
                                                    : 53));
    m_Talk.continues = continues;
    m_Talk.faces[0] = 6;
    m_Talk.faces[1] = 105;
    m_Talk.operation = 0;
    m_Talk.box_active = false;
    if (script == MIMA_SCRIPT_POST) {
        Hud_WipePlayfield();
        M_Music(music, BOSS_MUSIC_STOP);
        DialogueSequence_BeginBare(
            &m_Talk.sequence, patterns, &assets->stages[4], &m_Talk.runtime,
            nullptr, 0);
        DialogueSequence_Update(&m_Talk.sequence, nullptr);
        m_Talk.phase = MIMA_TALK_POST_RUN;
    } else {
        DialogueSequence_BeginPart(
            &m_Talk.sequence, patterns, &assets->stages[4], &m_Talk.runtime,
            nullptr, 0);
        m_Talk.phase =
            script == MIMA_SCRIPT_PRE ? MIMA_TALK_ENTER : MIMA_TALK_FORM_BEGIN;
    }
}

static bool M_RunScript(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_INPUT *const input)
{
    const DIALOGUE_SCRIPTS *const scripts = &assets->scripts;
    DIALOGUE_SEQUENCE *const sequence = &m_Talk.sequence;
    while (true) {
        switch (m_Talk.phase) {
        case MIMA_TALK_ENTER:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            Mima_SelectPatterns(patterns, &assets->mima, MIMA_BANK_SUMMON);
            DialogueSequence_NextPart(sequence, m_Talk.faces, 2);
            m_Talk.phase = MIMA_TALK_INTRO;
            break;
        case MIMA_TALK_INTRO:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            M_Delay(10, MIMA_TALK_APPEARANCE_BEGIN);
            break;
        case MIMA_TALK_APPEARANCE_BEGIN:
            DialogueSummon_Begin(
                &m_Talk.summon, patterns, BossScene_DrawSummon);
            m_Talk.phase = MIMA_TALK_APPEARANCE;
            break;
        case MIMA_TALK_APPEARANCE:
            if (!DialogueSummon_Update(&m_Talk.summon)) {
                return false;
            }
            Mima_SelectPatterns(patterns, &assets->mima, MIMA_BANK_FIRST);
            Mima_Place(boss);
            m_Talk.phase = MIMA_TALK_COMMON;
            break;
        case MIMA_TALK_COMMON:
            DialogueSequence_NextPart(sequence, m_Talk.faces, 2);
            m_Talk.phase = MIMA_TALK_BRANCH_BEGIN;
            break;
        case MIMA_TALK_BRANCH_BEGIN:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            if (m_Talk.continues != 0) {
                DialogueSequence_NextPart(
                    sequence, scripts->stage5_continued, 22);
            } else {
                m_Talk.runtime.box += 22;
                DialogueSequence_NextPart(
                    sequence, scripts->stage5_no_continue, 17);
            }
            m_Talk.phase = MIMA_TALK_BRANCH;
            break;
        case MIMA_TALK_BRANCH:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            if (m_Talk.continues != 0) {
                m_Talk.runtime.box += 17;
            }
            M_Fade(PALETTE_WHITE_OUT, 1, MIMA_TALK_WINGED_BEGIN);
            break;
        case MIMA_TALK_WINGED_BEGIN:
            Clip_SetPlayfield();
            Grcg_SetColor(0);
            Draw_BoxFill(g_Clip, 32, 0, 416, 399);
            Grcg_Off();
            Mima_DrawBody(boss, patterns);
            BossScene_DrawPlayer(patterns);
            M_Effect(10, false);
            BossScene_DrawBorder(patterns);
            M_Fade(PALETTE_WHITE_IN, 3, MIMA_TALK_WINGED);
            break;
        case MIMA_TALK_WINGED:
            M_Music(music, BOSS_MUSIC_START);
            DialogueSequence_NextPart(sequence, &m_Talk.faces[1], 1);
            sequence->leave = true;
            m_Talk.phase = MIMA_TALK_LEAVE;
            break;
        case MIMA_TALK_LEAVE:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            m_Talk.phase = MIMA_TALK_DONE;
            return true;
        case MIMA_TALK_FORM_BEGIN:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            if (m_Talk.continues != 0) {
                DialogueSequence_NextPart(
                    sequence, scripts->stage5_form_continued, 5);
            } else {
                m_Talk.runtime.box += 5;
                DialogueSequence_NextPart(
                    sequence, scripts->stage5_form_no_continue, 4);
            }
            m_Talk.phase = MIMA_TALK_FORM_BOXES;
            break;
        case MIMA_TALK_FORM_BOXES:
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            if (m_Talk.continues != 0) {
                M_Music(music, BOSS_MUSIC_FADE);
                M_Fade(PALETTE_WHITE_OUT, 10, MIMA_TALK_FORM_END);
            } else {
                Mima_SelectPatterns(patterns, &assets->mima, MIMA_BANK_SECOND);
                DialogueSequence_SlideOut(sequence);
                m_Talk.phase = MIMA_TALK_LEAVE;
            }
            break;
        case MIMA_TALK_FORM_END:
            M_Delay(50, MIMA_TALK_DONE);
            break;
        case MIMA_TALK_POST_RUN:
            ASSERT(m_Talk.operation < ARRAY_SIZE(m_Post));
            if (m_Post[m_Talk.operation].kind != M_BOX) {
                M_Post();
                break;
            }
            if (!m_Talk.box_active) {
                DialogueSequence_NextPart(sequence, &m_Talk.faces[1], 1);
                m_Talk.box_active = true;
            }
            if (!DialogueSequence_Update(sequence, input)) {
                return false;
            }
            m_Talk.box_active = false;
            m_Talk.operation++;
            break;
        case MIMA_TALK_FLASH_FORCE:
            M_Effect(5, true);
            m_Talk.operation++;
            M_Fade(PALETTE_WHITE_IN, 0, MIMA_TALK_POST_RUN);
            break;
        case MIMA_TALK_FADE:
        case MIMA_TALK_DELAY:
            if (!Wait_Ready(&m_Talk.wait, nullptr, 0)) {
                return false;
            }
            m_Talk.phase = m_Talk.after_wait;
            break;
        case MIMA_TALK_DONE:
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}

void Mima_TalkBegin(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post)
{
    (void)boss;
    (void)post;
    M_BeginScript(
        patterns, assets, music, session->continues_used, MIMA_SCRIPT_PRE);
}

bool Mima_TalkUpdate(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post,
    const TH02_INPUT *const input)
{
    (void)session;
    (void)post;
    const bool ready = M_RunScript(boss, patterns, assets, music, input);
    Stage_SetKeys(m_Talk.runtime.keys);
    return ready;
}

BOSS_RESULT Mima_Transition(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const TH02_INPUT *const input)
{
    if (assets->stages[4].boxes == nullptr) {
        return BOSS_WAIT;
    }
    const MIMA_TRANSITION transition = Mima_BeginTransition(boss, patterns);
    if (transition == MIMA_TRANSITION_ENDED) {
        return BOSS_WAIT;
    }
    if (transition != MIMA_TRANSITION_WAIT) {
        M_BeginScript(
            patterns, assets, music, session->continues_used,
            transition == MIMA_TRANSITION_FORM ? MIMA_SCRIPT_FORM_DEFEAT
                                               : MIMA_SCRIPT_POST);
    }
    const bool ready = M_RunScript(boss, patterns, assets, music, input);
    Stage_SetKeys(m_Talk.runtime.keys);
    if (!ready) {
        return BOSS_WAIT;
    }
    if (Mima_EndTransition(boss, session->continues_used != 0)
        == BOSS_FINISHED) {
        return BOSS_FINISHED;
    }
    return BOSS_RUNNING;
}
