#include "th01/stage.h"
#include "th01/pellets.h"
#include "th01/session.h"

#include "core/base/assert.h"

STAGE_ORB_COMPOSITE g_OrbComposite;

bool Stage_BeginOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets, const SHOTS_OUTPUT shots_output)
{
    ASSERT(!g_OrbComposite.active);
    g_OrbComposite.orb = Orb_BeginComposite(g_Session.stage_id, &g_Stage.combo);
    g_OrbComposite.active = true;
    g_OrbComposite.waiting = false;
    return Stage_ResumeOrbComposite(sprites, font, pellet_assets, shots_output);
}

bool Stage_ResumeOrbComposite(
    const PTN *const sprites, const GAME_TEXT_FONT *const font,
    const PELLET_ASSETS *const pellet_assets, const SHOTS_OUTPUT shots_output)
{
    if (!g_OrbComposite.active) {
        return true;
    }
    if (g_OrbComposite.waiting && !Frame_DelayReady(1)) {
        return false;
    }
    g_OrbComposite.waiting = false;
    for (;;) {
        const ORB_ACTION action =
            Orb_NextAction(&g_OrbComposite.orb, g_Player.is_hit);
        switch (action.kind) {
        case ORB_ACTION_DONE:
            g_OrbComposite.active = false;
            return true;
        case ORB_ACTION_DELAY_ONE:
            if (!Frame_DelayReady(1)) {
                g_OrbComposite.waiting = true;
                return false;
            }
            break;
        case ORB_ACTION_PELLETS:
            Pellets_UpdateAndRender(pellet_assets, shots_output);
            break;
        case ORB_ACTION_CARDS_HITTEST:
            Stage_HitTestCards();
            break;
        case ORB_ACTION_OBSTACLES:
            Stage_UpdateObstacles(sprites, false);
            break;
        case ORB_ACTION_PLAYER_HITTEST: {
            const ORB_PLAYER player = {
                .left = g_Player.left,
                .top = PLAYER_TOP,
                .invincible = g_Player.invincible,
                .invincible_against_orb = g_Player.invincible_against_orb,
            };
            if (Orb_PlayerHitTest(&player, ORB_REPEL_NONE)) {
                g_Player.is_hit = true;
            }
            break;
        }
        case ORB_ACTION_CARDS_RENDER:
            Stage_UpdateCards(sprites, font);
            break;
        case ORB_ACTION_ERASE:
        case ORB_ACTION_PUT:
            Orb_RenderAction(sprites, action);
            break;
        }
    }
}
