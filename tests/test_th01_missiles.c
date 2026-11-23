#include "th01/missiles.h"

#include "th01_video.h"
#include "test.h"

typedef struct {
    MISSILES_ACTION actions[256];
    int32_t count;
} EVENTS;

static void M_Action(void *const user, const MISSILES_ACTION action)
{
    EVENTS *const events = user;
    CHECK(events->count < 256);
    events->actions[events->count++] = action;
}

TEST(pool_and_spawn_history)
{
    MISSILES m = {};
    m.slots[0].previous_top = 123;
    m.slots[0].unused = 456;
    for (int32_t i = 0; i < 50; i++) {
        CHECK(Missiles_Add(&m, 100, 200, -1.99, 0.99, -7));
        CHECK_EQ_INT(m.slots[i].left, 1600);
        CHECK_EQ_INT(m.slots[i].velocity.x, -31);
        CHECK_EQ_INT(m.slots[i].velocity.y, 15);
        CHECK_EQ_INT(m.slots[i].previous_left, -1);
        CHECK_EQ_INT(m.slots[i].reserved, -7);
    }
    CHECK(!Missiles_Add(&m, 0, 0, 0, 0, 0));
    CHECK_EQ_INT(m.slots[0].previous_top, 123);
    CHECK_EQ_INT(m.slots[0].unused, 456);
    Missiles_Reset(&m);
    CHECK_EQ_INT(m.slots[0].left, 1600);
    for (int32_t i = 0; i < 50; i++) {
        CHECK_EQ_INT(m.slots[i].flag, MISSILE_FREE);
    }
}

TEST(interlace_motion_and_explosion_lifetime)
{
    MISSILES m = {};
    PLAYER player = { .left = 300 };
    EVENTS events = {};
    const MISSILES_OUTPUT output = { M_Action, &events };
    CHECK(Missiles_Add(&m, 623, 100, 2, 0, 0));
    m.slots[0].previous_top = 80 * 16;
    Missiles_UpdateAndRender(&m, &player, 1, output);
    CHECK_EQ_INT(m.slots[0].left, 625 * 16);
    CHECK_EQ_INT(m.slots[0].previous_left, 624 * 16);
    CHECK_EQ_INT(m.slots[0].previous_top, 80 * 16);
    CHECK_EQ_INT(m.slots[0].flag, MISSILE_HIT);
    CHECK_EQ_INT(events.count, 1);
    CHECK_EQ_INT(events.actions[0].left, -1);
    CHECK_EQ_INT(events.actions[0].top, 80);
    events.count = 0;
    Missiles_UpdateAndRender(&m, &player, 0, output);
    CHECK_EQ_INT(m.slots[0].flag, MISSILE_HIT_LAST);
    CHECK_EQ_INT(events.count, 2);
    CHECK_EQ_INT(events.actions[1].image, 4);
    CHECK_EQ_INT(events.actions[1].quarter, 1);
    events.count = 0;
    Missiles_UpdateAndRender(&m, &player, 1, output);
    CHECK_EQ_INT(events.count, 0);
    Missiles_UpdateAndRender(&m, &player, 0, output);
    CHECK_EQ_INT(m.slots[0].flag, MISSILE_FREE);
    CHECK_EQ_INT(events.count, 3);
    CHECK_EQ_INT(events.actions[2].kind, MISSILES_ACTION_EFFECT);
    CHECK_EQ_INT(events.actions[2].effect, 7);
}

TEST(lethal_current_coordinates_ignore_field_and_sliding)
{
    const int16_t xs[] = { 292, 293, 323, 324 };
    const int16_t ys[] = { 360, 361, 399, 400 };
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            MISSILES m = {};
            m.slots[0] = (MISSILE) {
                .left = xs[x] * 16,
                .top = ys[y] * 16,
                .previous_left = 100 * 16,
                .previous_top = 100 * 16,
                .flag = MISSILE_HIT,
            };
            PLAYER player = { .left = 300, .sliding = true };
            EVENTS events = {};
            Missiles_UpdateAndRender(
                &m, &player, 1, (MISSILES_OUTPUT) { M_Action, &events });
            CHECK(player.is_hit == (x > 0 && x < 3 && y > 0 && y < 3));
            player.is_hit = false;
            player.invincible = true;
            Missiles_UpdateAndRender(
                &m, &player, 1, (MISSILES_OUTPUT) { M_Action, &events });
            CHECK(!player.is_hit);
        }
    }
}

TEST(unaligned_quarters_and_word_rounded_restore)
{
    uint8_t pixels[5 * 1024];
    memset(pixels, 15, sizeof(pixels));
    pixels[0] = 3;
    pixels[15] = 5;
    PTN sprites = { .count = 5, .pixels = pixels };
    CHECK(IS_OK(Missiles_ValidateSprites(&sprites)));
    Video_Init(&g_HeapAllocator);
    Missiles_RenderAction(
        &sprites,
        (MISSILES_ACTION) {
            .kind = MISSILES_ACTION_PUT, .left = 19, .top = 100 });
    CHECK_EQ_INT(Th01Test_Pixel(0, 19, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 34, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 100), 0);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    Missiles_RenderAction(
        &sprites,
        (MISSILES_ACTION) {
            .kind = MISSILES_ACTION_RESTORE, .left = 19, .top = 100 });
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 100), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 63, 115), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 64, 115), 0);
    Mem_Free(g_Video.pages[0]);
}

TEST(new_bottom_explosion_keeps_machine_shift_behavior)
{
    uint8_t pixels[5 * 1024];
    memset(pixels, 15, sizeof(pixels));
    pixels[4 * 1024 + 9] = 5;
    PTN sprites = { .count = 5, .pixels = pixels };
    Video_Init(&g_HeapAllocator);
    Missiles_RenderAction(
        &sprites,
        (MISSILES_ACTION) { .kind = MISSILES_ACTION_PUT,
                            .left = -1,
                            .top = 392,
                            .image = 4,
                            .quarter = 0 });
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 392), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 392), 0);
    Mem_Free(g_Video.pages[0]);
}
