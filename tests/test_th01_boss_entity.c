#include "th01/boss_entity.h"

#include "test.h"

TEST(shared_slots_rebind_without_changing_other_metadata)
{
    const uint8_t pixels[16] = {};
    const uint8_t coverage[16] = {};
    const BOS first = { 16, 1, 1, pixels, coverage };
    const BOS second = { 16, 1, 1, coverage, pixels };
    BOSS_SPRITES bank = {};
    BOSS_ENTITY a = { .image = 5, .left = 100 };
    BOSS_ENTITY b = { .image = 3, .left = 200 };
    CHECK(IS_OK(BossEntity_Bind(&a, &bank, 2, &first)));
    BossEntity_CopyMetadata(&b, &a);
    CHECK_EQ_INT(b.image, 3);
    CHECK_EQ_INT(b.left, 200);
    CHECK_EQ_INT(b.sprites.slot, 2);
    CHECK(IS_OK(BossEntity_Bind(&a, &bank, 2, &second)));
    CHECK(bank.entities[b.sprites.slot] == &second);
    CHECK_EQ_INT(a.image, 5);
    BOSS_ANIMATION animation = {};
    CHECK(IS_OK(BossAnimation_Bind(&animation, &bank, 1, &first)));
    CHECK(bank.animations[1] == &first);
    CHECK(bank.entities[2] == &second);
    BOS invalid = first;
    invalid.count = 28;
    RESULT result = BossEntity_Bind(&a, &bank, 2, &invalid);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(bank.entities[2] == &second);
    CHECK_EQ_INT(a.sprites.count, 1);
    result = BossAnimation_Bind(&animation, &bank, 2, &first);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
}

TEST(orb_hit_uses_previous_position_and_inclusive_edges)
{
    BOSS_ENTITY entity = { .left = 100, .top = 120 };
    BossEntity_SetOrbHitbox(&entity, (BOSS_BOUNDS) { 0, 96, 0, 96 });
    ORB orb = {
        .left = 600, .top = 368, .previous_left = 92, .previous_top = 112
    };
    CHECK(BossEntity_HitTestOrb(&entity, &orb));
    orb.previous_left = 172;
    orb.previous_top = 192;
    CHECK(BossEntity_HitTestOrb(&entity, &orb));
    orb.previous_left++;
    CHECK(!BossEntity_HitTestOrb(&entity, &orb));
    orb.previous_left = 92;
    entity.orb_inactive = true;
    CHECK(!BossEntity_HitTestOrb(&entity, &orb));
}

TEST(movement_wraps_then_applies_both_clamp_edges)
{
    BOSS_ENTITY entity = {
        .previous_left = 7, .previous_top = 9, .image = 3, .lock_frame = 2
    };
    BossEntity_SetPosition(
        &entity, 32760, -32760, (BOSS_BOUNDS) { -30000, 30000, 100, -100 });
    CHECK_EQ_INT(entity.previous_left, 7);
    CHECK_EQ_INT(entity.previous_top, 9);
    BossEntity_Move(&entity, 16, -16);
    CHECK_EQ_INT(entity.left, -30000);
    CHECK_EQ_INT(entity.top, -100);
    CHECK_EQ_INT(entity.previous_left, 32760);
    CHECK_EQ_INT(entity.previous_top, -32760);
    CHECK_EQ_INT(entity.image, 3);
    CHECK_EQ_INT(entity.lock_frame, 2);
}
