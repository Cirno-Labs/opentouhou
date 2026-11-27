#include "th02/boss/talk.h"

#include "th02/background.h"
#include "th02/boss/strategy.h"
#include "th02/clip.h"
#include "th02/dialogue/sequence.h"
#include "th02/palette.h"
#include "th02/player.h"
#include "th02/stage.h"
#include "th02/stage/private.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

// The common flow's own phases; a boss with begin/update overrides never
// visits these.
typedef enum {
    BOSS_TALK_EXTRA_ENTER,
    BOSS_TALK_INTRO,
    BOSS_TALK_WHITE_OUT,
    BOSS_TALK_WHITE_IN,
    BOSS_TALK_GENERIC,
    BOSS_TALK_DONE,
} BOSS_TALK_PHASE;

typedef struct {
    DIALOGUE_SEQUENCE sequence;
    DIALOGUE_RUNTIME runtime;
    WAIT wait;
    BOSS_TALK_PHASE phase;
    bool started;
    bool post;
    bool music_started;
    bool prepare_started;
    bool prepared;
} BOSS_TALK;

static BOSS_TALK m_Talk;

static bool M_DialogueStep(const TH02_INPUT *const input)
{
    const bool ready = DialogueSequence_Update(&m_Talk.sequence, input);
    Stage_SetKeys(m_Talk.runtime.keys);
    return ready;
}

static void M_Fade(const PALETTE_FADE_KIND kind, const BOSS_TALK_PHASE phase)
{
    Wait_Fade(&m_Talk.wait, kind, 1);
    m_Talk.phase = phase;
}

static bool M_FadeReady(void)
{
    return Wait_Ready(&m_Talk.wait, nullptr, 0);
}

static void M_BeginGeneric(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post)
{
    (void)boss;
    (void)music;
    DialogueSequence_Begin(
        &m_Talk.sequence, patterns, &assets->stages[session->stage],
        &assets->scripts, &m_Talk.runtime, session->stage, post);
    m_Talk.phase = BOSS_TALK_GENERIC;
}

static void M_Begin(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post,
    const BOSS_TALK_HOOKS *const talk)
{
    if (post) {
        M_BeginGeneric(boss, patterns, assets, music, session, post);
        return;
    }
    if (talk->extra_background) {
        Background_SetMode(TILE_DISABLED);
        DialogueSequence_BeginPart(
            &m_Talk.sequence, patterns, &assets->stages[session->stage],
            &m_Talk.runtime, nullptr, 0);
        m_Talk.phase = BOSS_TALK_EXTRA_ENTER;
        return;
    }
    if (talk->place != nullptr) {
        DialogueSequence_BeginIntro(
            &m_Talk.sequence, patterns, &assets->stages[session->stage],
            &m_Talk.runtime, session->stage);
        m_Talk.phase = BOSS_TALK_INTRO;
        return;
    }
    M_BeginGeneric(boss, patterns, assets, music, session, post);
}

static bool M_UpdateCommon(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post,
    const TH02_INPUT *const input, const BOSS_TALK_HOOKS *const talk)
{
    const uint8_t stage = session->stage;
    DIALOGUE_SEQUENCE *const sequence = &m_Talk.sequence;
    if (m_Talk.phase == BOSS_TALK_EXTRA_ENTER) {
        if (!M_DialogueStep(input)) {
            return false;
        }
        if (talk->select_patterns != nullptr) {
            talk->select_patterns(patterns, assets, 0);
        }
        Clip_SetPlayfield();
        sequence->intro_faces[0] = 255;
        sequence->intro_faces[1] = 0;
        sequence->intro_faces[2] = 255;
        DialogueSequence_NextPart(sequence, sequence->intro_faces, 3);
        m_Talk.phase = BOSS_TALK_INTRO;
    }
    if (m_Talk.phase == BOSS_TALK_INTRO) {
        if (!DialogueSequence_Update(sequence, input)) {
            Stage_SetKeys(m_Talk.runtime.keys);
            return false;
        }
        ASSERT(talk->place != nullptr);
        talk->place(boss);
        M_Fade(PALETTE_WHITE_OUT, BOSS_TALK_WHITE_OUT);
    }
    if (m_Talk.phase == BOSS_TALK_WHITE_OUT) {
        if (!M_FadeReady()) {
            return false;
        }
        ASSERT(talk->draw_scene != nullptr);
        talk->draw_scene(boss, patterns);
        if (music->boss != nullptr) {
            music->boss(music->context);
            m_Talk.music_started = true;
        }
        M_Fade(PALETTE_WHITE_IN, BOSS_TALK_WHITE_IN);
    }
    if (m_Talk.phase == BOSS_TALK_WHITE_IN) {
        if (!M_FadeReady()) {
            return false;
        }
        DialogueSequence_Continue(sequence, &assets->scripts, stage);
        m_Talk.phase = BOSS_TALK_GENERIC;
    }
    const bool ready = DialogueSequence_Update(sequence, input);
    Stage_SetKeys(m_Talk.runtime.keys);
    if (ready && !post && talk->music_after && !m_Talk.music_started
        && music->boss != nullptr) {
        music->boss(music->context);
        m_Talk.music_started = true;
    }
    return ready;
}

void BossTalk_BeginStage(void)
{
    memset(&m_Talk, 0, sizeof(m_Talk));
}

bool BossTalk_Prepare(const PATTERN_BANK *const patterns)
{
    if (m_Talk.prepared) {
        return true;
    }
    STAGE_SCROLL *const scroll = &p_Stage->scroll;
    ASSERT(scroll->front < 2 && scroll->back < 2);
    if (!m_Talk.prepare_started) {
        Player_SyncPositionY(scroll->front);
        Stage_ResetScrollPosition();
        Video_AccessPage(0);
        Video_Clear();
        Video_AccessPage(1);
        Video_Clear();
        Wait_Delay(&m_Talk.wait, 1);
        m_Talk.prepare_started = true;
        return false;
    }
    if (!Wait_Ready(&m_Talk.wait, nullptr, 0)) {
        return false;
    }
    Video_ScrollUp(0);
    Video_AccessPage(scroll->front);
    Draw_SpriteRoll(
        &patterns->entries[0], g_Player->current.x, g_Player->current.y);
    Player_PlaceOption(scroll->back);
    Player_SyncOptions(scroll->back, scroll->front);
    const ENTITY_POINT option = g_Player->option[scroll->back];
    const PATTERN *const pattern = &patterns->entries[72];
    Draw_SpriteRollTiny(pattern, option.x, option.y);
    Draw_SpriteRollTiny(pattern, Math16_Wrap((int32_t)option.x + 48), option.y);
    m_Talk.prepared = true;
    return true;
}

bool BossTalk_Update(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post,
    const TH02_INPUT *const input)
{
    const uint8_t stage = session->stage;
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(stage);
    if (strategy == nullptr) {
        return true;
    }
    const BOSS_TALK_HOOKS *const talk = &strategy->talk;
    if (!post && !m_Talk.started && talk->prepare != nullptr) {
        talk->prepare(boss);
    }
    if (assets->stages[stage].boxes == nullptr) {
        return true;
    }
    if (!m_Talk.started || m_Talk.post != post) {
        if (!post && stage >= 3 && !BossTalk_Prepare(patterns)) {
            return false;
        }
        m_Talk.started = true;
        m_Talk.post = post;
        if (!post && talk->music_before && music->boss != nullptr) {
            music->boss(music->context);
            m_Talk.music_started = true;
        }
        if (talk->begin != nullptr) {
            talk->begin(boss, patterns, assets, music, session, post);
        } else {
            M_Begin(boss, patterns, assets, music, session, post, talk);
        }
    }
    if (talk->update != nullptr) {
        return talk->update(
            boss, patterns, assets, music, session, post, input);
    }
    return M_UpdateCommon(
        boss, patterns, assets, music, session, post, input, talk);
}
