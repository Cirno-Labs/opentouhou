#include "th01/stage.h"
#include "th01/pellets.h"
#include "th01/session.h"

#include "core/base/assert.h"

STAGE_ORB_COMPOSITE g_OrbComposite;

bool Stage_BeginOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets)
{
    ASSERT(!g_OrbComposite.active);
    Orb_AdvanceMotion(&g_Stage.combo);
    g_OrbComposite.boss = g_Session.stage_id % 5 == 4;
    g_OrbComposite.active = true;
    g_OrbComposite.waiting = false;
    return Stage_ResumeOrbComposite(sprites, font, pellet_assets);
}

bool Stage_ResumeOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets)
{
    if (!g_OrbComposite.active) {
        return true;
    }
    if (g_OrbComposite.waiting) {
        if (!Frame_DelayReady(1)) {
            return false;
        }
        g_OrbComposite.waiting = false;
    } else if (!Frame_DelayReady(1)) {
        g_OrbComposite.waiting = true;
        return false;
    }
    Pellets_UpdateAndRender(pellet_assets, sprites);
    if (!g_OrbComposite.boss) {
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
    if (!g_OrbComposite.boss) {
        Stage_UpdateCards(sprites, font);
    }
    if (!g_Orb.in_portal && !g_Player.is_hit) {
        Orb_Put(sprites, g_Orb.left, g_Orb.top, Orb_Image());
    }
    g_Orb.previous_left = g_Orb.left;
    g_Orb.previous_top = g_Orb.top;
    g_OrbComposite.active = false;
    return true;
}
