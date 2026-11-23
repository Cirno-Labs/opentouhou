#include "th01/stage.h"

#include "core/base/assert.h"

STAGE_ORB_COMPOSITE g_OrbComposite;

bool Stage_BeginOrbComposite(const STAGE_CONTEXT *const context)
{
    ASSERT(!g_OrbComposite.active);
    g_OrbComposite.orb = Orb_BeginComposite(context->stage_id, &g_Stage.combo);
    g_OrbComposite.active = true;
    g_OrbComposite.waiting = false;
    return Stage_ResumeOrbComposite(context);
}

bool Stage_ResumeOrbComposite(const STAGE_CONTEXT *const context)
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
            context->pellets_update(context->user);
            break;
        case ORB_ACTION_CARDS_HITTEST:
            Stage_HitTestCards(context);
            break;
        case ORB_ACTION_OBSTACLES:
            Stage_UpdateObstacles(context, false);
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
            Stage_UpdateCards(context);
            break;
        case ORB_ACTION_ERASE:
        case ORB_ACTION_PUT:
            Orb_RenderAction(context->sprites, action);
            break;
        }
    }
}
