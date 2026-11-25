#include "th01/stage.h"
#include "th01/pellets.h"
#include "th01/session.h"

#include "core/base/assert.h"

#include <string.h>

static STAGE_ORB_COMPOSITE m_OrbComposite;

void StageOrb_Init(void)
{
    memset(&m_OrbComposite, 0, sizeof(m_OrbComposite));
}

bool StageOrb_Waiting(void)
{
    return m_OrbComposite.waiting;
}

bool Stage_BeginOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets)
{
    ASSERT(!m_OrbComposite.active);
    Orb_AdvanceMotion(&g_Stage.combo);
    m_OrbComposite.boss = g_Session.stage_id % 5 == 4;
    m_OrbComposite.active = true;
    m_OrbComposite.waiting = false;
    return Stage_ResumeOrbComposite(sprites, font, pellet_assets);
}

bool Stage_ResumeOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets)
{
    if (!m_OrbComposite.active) {
        return true;
    }
    if (m_OrbComposite.waiting) {
        if (!VSyncIrq_DelayReady(1)) {
            return false;
        }
        m_OrbComposite.waiting = false;
    } else if (!VSyncIrq_DelayReady(1)) {
        m_OrbComposite.waiting = true;
        return false;
    }
    Pellets_UpdateAndRender(pellet_assets, sprites);
    if (!m_OrbComposite.boss) {
        Stage_HitTestCards();
        Stage_UpdateObstacles(sprites, false);
    }
    if (!g_Orb.in_portal) {
        const ORB_PLAYER player = {
            .left = g_Player.left,
            .top = PLAYER_TOP,
            .invincible = g_Player.invincible,
            .invincible_against_orb = g_Player.invincible_against_orb,
        };
        if (Orb_PlayerHitTest(&player, ORB_REPEL_NONE)) {
            g_Player.is_hit = true;
        }
        Orb_Erase(
            sprites, g_Orb.previous_left, g_Orb.previous_top, Orb_Image());
    }
    if (!m_OrbComposite.boss) {
        Stage_UpdateCards(sprites, font);
    }
    if (!g_Orb.in_portal && !g_Player.is_hit) {
        Orb_Put(sprites, g_Orb.left, g_Orb.top, Orb_Image());
    }
    g_Orb.previous_left = g_Orb.left;
    g_Orb.previous_top = g_Orb.top;
    m_OrbComposite.active = false;
    return true;
}
