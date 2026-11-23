#include "th01/frame.h"
#include "pc98/vsync.h"
#include "th01/logo.h"

#include "core/base/arena.h"

#include "test.h"

static uint64_t M_ReferenceCount(const uint64_t host_ticks)
{
    return host_ticks * 21052600 / (60 * 8 * 106 * 440);
}

static void M_CheckLogo(const LOGO *const actual, const LOGO *const expected)
{
    CHECK_EQ_INT(actual->frame, expected->frame);
    CHECK_EQ_INT(actual->tone, expected->tone);
    CHECK_EQ_INT(actual->sprite, expected->sprite);
    CHECK_EQ_INT(actual->period, expected->period);
    CHECK_EQ_INT(actual->phase, expected->phase);
    CHECK_EQ_INT(actual->amplitude, expected->amplitude);
    CHECK_EQ_INT(actual->star_phase, expected->star_phase);
    CHECK_EQ_INT(actual->random, expected->random);
    CHECK_EQ_INT(actual->half_tick, expected->half_tick);
    for (int32_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(actual->circles[i].x, expected->circles[i].x);
        CHECK_EQ_INT(actual->circles[i].y, expected->circles[i].y);
        CHECK_EQ_INT(actual->velocity[i].x, expected->velocity[i].x);
        CHECK_EQ_INT(actual->velocity[i].y, expected->velocity[i].y);
    }
    for (int32_t i = 0; i < 50; i++) {
        CHECK_EQ_INT(actual->stars[i].x, expected->stars[i].x);
        CHECK_EQ_INT(actual->stars[i].y, expected->stars[i].y);
        CHECK_EQ_INT(actual->speeds[i], expected->speeds[i]);
    }
    CHECK_EQ_INT(actual->video->drawing_page, expected->video->drawing_page);
    CHECK_EQ_INT(actual->video->visible_page, expected->video->visible_page);
    for (int32_t page = 0; page < 2; page++) {
        CHECK(
            memcmp(
                actual->video->pages[page], expected->video->pages[page],
                VIDEO_PAGE_SIZE)
            == 0);
    }
    CHECK(
        memcmp(
            actual->video->palette, expected->video->palette,
            sizeof(actual->video->palette))
        == 0);
}

static bool M_AdvanceLogo(
    LOGO *const logo, const BFNT *const sprites, int32_t *const changed)
{
    const bool running = Logo_Tick(logo, sprites, changed);
    if (*changed >= 0) {
        GFX_COLOR palette[16];
        Logo_Palette(logo, sprites, palette);
        Video_SetPalette(logo->video, palette);
    }
    return running;
}

TEST(scheduled_logo_preserves_every_logical_update_and_page)
{
    ARENA arena;
    Arena_Init(&arena, "scheduled logo", 2 * 1024 * 1024);
    VIDEO actual_video, expected_video;
    Video_Init(&actual_video, Arena_Allocator(&arena));
    Video_Init(&expected_video, Arena_Allocator(&arena));
    LOGO actual, expected;
    Logo_Init(&actual, &actual_video);
    Logo_Init(&expected, &expected_video);
    BFNT sprites = { .width = 64, .height = 16, .count = 16 };
    sprites.pixels = Arena_Alloc(&arena, 64 * 16 * 16);
    for (int32_t i = 0; i < 64 * 16 * 16; i++) {
        sprites.pixels[i] = (uint8_t)((i * 7 + i / 13) % 16);
    }
    for (int32_t i = 0; i < 16; i++) {
        sprites.palette[i] =
            (GFX_COLOR) { (uint8_t)(i * 17), (uint8_t)(255 - i * 17),
                          (uint8_t)(i * 13), 255 };
    }
    VSYNC_CLOCK clock = {};
    int32_t logical_ticks = 0;
    int32_t host_ticks = 0;
    int32_t changed_pages = 0;
    bool running = true;
    while (running && host_ticks < 600) {
        host_ticks++;
        int32_t actual_changed = -1;
        int32_t expected_changed = -1;
        bool expected_running = true;
        if (M_ReferenceCount(host_ticks) != M_ReferenceCount(host_ticks - 1)) {
            expected_running =
                M_AdvanceLogo(&expected, &sprites, &expected_changed);
        }
        if (VSync_Advance(&clock)) {
            running = M_AdvanceLogo(&actual, &sprites, &actual_changed);
            logical_ticks += running;
        }
        CHECK_EQ_INT(running, expected_running);
        CHECK_EQ_INT(actual_changed, expected_changed);
        changed_pages += actual_changed >= 0;
        M_CheckLogo(&actual, &expected);
    }
    CHECK(!running);
    CHECK_EQ_INT(host_ticks, 492);
    CHECK_EQ_INT(logical_ticks, 461);
    CHECK_EQ_INT(changed_pages, 231);
    Arena_Free(&arena);
}

TEST(raw_wait_requires_next_edge_and_delay_reuses_it)
{
    FRAME_VSYNC vsync = { .count1 = 37, .count2 = -20 };
    FRAME_WAIT wait;
    Frame_BeginWait(&wait, &vsync);
    CHECK(!Frame_WaitReady(&wait, &vsync));
    CHECK(!Frame_WaitReady(&wait, &vsync));
    CHECK_EQ_INT(vsync.count1, 37);
    Frame_VSync(&vsync);
    CHECK(Frame_WaitReady(&wait, &vsync));
    CHECK_EQ_INT(vsync.count1, 38);
    CHECK_EQ_INT(vsync.count2, -19);
    CHECK(Frame_DelayReady(&vsync, 1));
    CHECK_EQ_INT(vsync.count1, 0);
    CHECK_EQ_INT(vsync.count2, -19);
    CHECK(!Frame_DelayReady(&vsync, 1));
    Frame_BeginWait(&wait, &vsync);
    CHECK(!Frame_WaitReady(&wait, &vsync));
    Frame_VSync(&vsync);
    CHECK(Frame_WaitReady(&wait, &vsync));
    CHECK(Frame_DelayReady(&vsync, 1));
}

TEST(delay_resets_on_exit_and_preserves_unsigned_comparison)
{
    FRAME_VSYNC vsync = { .count1 = 1, .count2 = 32767 };
    CHECK(!Frame_DelayReady(&vsync, 2));
    CHECK_EQ_INT(vsync.count1, 1);
    Frame_VSync(&vsync);
    CHECK_EQ_INT(vsync.count2, -32768);
    CHECK(Frame_DelayReady(&vsync, 2));
    CHECK_EQ_INT(vsync.count1, 0);
    vsync.count1 = 32767;
    Frame_VSync(&vsync);
    CHECK_EQ_INT(vsync.count1, -32768);
    CHECK(Frame_DelayReady(&vsync, 32768));
    vsync.count1 = -1;
    CHECK(Frame_DelayReady(&vsync, 65535));
    vsync.count1 = -1;
    Frame_VSync(&vsync);
    CHECK_EQ_INT(vsync.count1, 0);
    CHECK(!Frame_DelayReady(&vsync, 65535));
    vsync.count1 = 19;
    CHECK(Frame_DelayReady(&vsync, 0));
    CHECK_EQ_INT(vsync.count1, 0);
}

TEST(program_switch_counts_the_latched_vsync)
{
    FRAME_VSYNC vsync = { .count1 = 37, .count2 = -20, .edge = 5 };
    Frame_SwitchProgram(&vsync);
    CHECK_EQ_INT(vsync.count1, 1);
    CHECK_EQ_INT(vsync.count2, 1);
    CHECK_EQ_INT((int32_t)vsync.edge, 5);
    CHECK(Frame_DelayReady(&vsync, 1));
}
