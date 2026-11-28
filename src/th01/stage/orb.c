#include "th01/pellets.h"
#include "th01/session.h"
#include "th01/stage/private.h"

#include "core/base/assert.h"

#include <string.h>

STAGE_ORB_COMPOSITE p_OrbComposite;

void StageOrb_Init(void)
{
    memset(&p_OrbComposite, 0, sizeof(p_OrbComposite));
}

bool Stage_BeginOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets)
{
    ASSERT(!p_OrbComposite.active);
    Orb_AdvanceMotion(&p_Stage->combo);
    p_OrbComposite.boss = g_Session.stage_id % 5 == 4;
    p_OrbComposite.active = true;
    p_OrbComposite.waiting = false;
    return Stage_ResumeOrbComposite(sprites, font, pellet_assets);
}

bool Stage_ResumeOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets)
{
    if (!p_OrbComposite.active) {
        return true;
    }
    if (p_OrbComposite.waiting) {
        if (!VSyncIrq_DelayReady(1)) {
            return false;
        }
        p_OrbComposite.waiting = false;
    } else if (!VSyncIrq_DelayReady(1)) {
        p_OrbComposite.waiting = true;
        return false;
    }
    Pellets_Update(pellet_assets, sprites);
    if (!p_OrbComposite.boss) {
        Stage_HitTestCards();
        Stage_UpdateObstacles(sprites, false);
    }
    if (!g_Orb->in_portal) {
        const ORB_PLAYER player = {
            .left = g_Player->left,
            .top = PLAYER_TOP,
            .invincible = g_Player->invincible,
            .invincible_against_orb = g_Player->invincible_against_orb,
        };
        if (Orb_PlayerHitTest(&player, ORB_REPEL_NONE)) {
            Player_Hit();
        }
        Orb_Erase(
            sprites, g_Orb->previous_left, g_Orb->previous_top, Orb_Image());
    }
    if (!p_OrbComposite.boss) {
        Stage_UpdateCards(sprites, font);
    }
    if (!g_Orb->in_portal && !g_Player->is_hit) {
        Orb_Put(sprites, g_Orb->left, g_Orb->top, Orb_Image());
    }
    Orb_KeepPosition();
    p_OrbComposite.active = false;
    return true;
}
