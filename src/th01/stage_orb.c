#include "th01/stage.h"

#include "core/base/assert.h"

bool Stage_BeginOrbComposite(
    STAGE_ORB_COMPOSITE *const composite, const STAGE_CONTEXT *const context)
{
    ASSERT(!composite->active);
    composite->orb = Orb_BeginComposite(
        context->orb, context->stage_id, context->rng,
        &context->gameplay->combo);
    composite->active = true;
    composite->waiting = false;
    return Stage_ResumeOrbComposite(composite, context);
}

bool Stage_ResumeOrbComposite(
    STAGE_ORB_COMPOSITE *const composite, const STAGE_CONTEXT *const context)
{
    if (!composite->active) {
        return true;
    }
    if (composite->waiting && !Frame_DelayReady(1)) {
        return false;
    }
    composite->waiting = false;
    for (;;) {
        const ORB_ACTION action = Orb_NextAction(
            context->orb, &composite->orb, context->player->is_hit);
        switch (action.kind) {
        case ORB_ACTION_DONE:
            composite->active = false;
            return true;
        case ORB_ACTION_DELAY_ONE:
            if (!Frame_DelayReady(1)) {
                composite->waiting = true;
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
                .left = context->player->left,
                .top = PLAYER_TOP,
                .invincible = context->player->invincible,
                .invincible_against_orb =
                    context->player->invincible_against_orb,
            };
            if (Orb_PlayerHitTest(
                    context->orb, &player, ORB_REPEL_NONE, context->rng)) {
                context->player->is_hit = true;
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
