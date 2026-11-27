#include "tests/test.h"
#include "th02/clip.h"

TEST(initial_bounds_cover_the_screen_inclusively)
{
    CLIP clip;
    Clip_Init(&clip);
    CHECK_EQ_INT(clip.left, 0);
    CHECK_EQ_INT(clip.top, 0);
    CHECK_EQ_INT(clip.right, 639);
    CHECK_EQ_INT(clip.bottom, 399);
}

TEST(reversed_pairs_and_screen_overshoot_are_normalized)
{
    CLIP clip;
    Clip_Init(&clip);
    CHECK(Result_Ignore(Clip_Set(&clip, 500, 350, -20, -10)));
    CHECK_EQ_INT(clip.left, 0);
    CHECK_EQ_INT(clip.top, 0);
    CHECK_EQ_INT(clip.right, 500);
    CHECK_EQ_INT(clip.bottom, 350);
    CHECK(Result_Ignore(Clip_Set(&clip, 600, 380, INT16_MAX, INT16_MAX)));
    CHECK_EQ_INT(clip.left, 600);
    CHECK_EQ_INT(clip.top, 380);
    CHECK_EQ_INT(clip.right, 639);
    CHECK_EQ_INT(clip.bottom, 399);
    CHECK(Result_Ignore(
        Clip_Set(&clip, INT16_MIN, INT16_MAX, INT16_MAX, INT16_MIN)));
    CHECK_EQ_INT(clip.left, 0);
    CHECK_EQ_INT(clip.top, 0);
    CHECK_EQ_INT(clip.right, 639);
    CHECK_EQ_INT(clip.bottom, 399);
}

TEST(single_pixel_bounds_are_valid_at_both_screen_corners)
{
    CLIP clip;
    Clip_Init(&clip);
    CHECK(Result_Ignore(Clip_Set(&clip, 0, 0, 0, 0)));
    CHECK_EQ_INT(clip.left, clip.right);
    CHECK_EQ_INT(clip.top, clip.bottom);
    CHECK(Result_Ignore(Clip_Set(&clip, 639, 399, 639, 399)));
    CHECK_EQ_INT(clip.left, 639);
    CHECK_EQ_INT(clip.top, 399);
    CHECK_EQ_INT(clip.right, 639);
    CHECK_EQ_INT(clip.bottom, 399);
}

TEST(x_failure_preserves_every_bound)
{
    CLIP clip;
    Clip_Init(&clip);
    CHECK(Result_Ignore(Clip_Set(&clip, 10, 20, 30, 40)));
    const CLIP saved = clip;
    CHECK(!Result_Ignore(Clip_Set(&clip, -1, 0, INT16_MIN, 100)));
    CHECK(memcmp(&clip, &saved, sizeof(clip)) == 0);
    CHECK(!Result_Ignore(Clip_Set(&clip, 640, 0, INT16_MAX, 100)));
    CHECK(memcmp(&clip, &saved, sizeof(clip)) == 0);
}

TEST(y_failure_keeps_new_x_and_previous_y_bounds)
{
    CLIP clip;
    Clip_Init(&clip);
    CHECK(Result_Ignore(Clip_Set(&clip, 10, 20, 30, 40)));
    CHECK(!Result_Ignore(Clip_Set(&clip, 300, -1, 200, INT16_MIN)));
    CHECK_EQ_INT(clip.left, 200);
    CHECK_EQ_INT(clip.right, 300);
    CHECK_EQ_INT(clip.top, 20);
    CHECK_EQ_INT(clip.bottom, 40);
    CHECK(!Result_Ignore(Clip_Set(&clip, 0, 400, 639, INT16_MAX)));
    CHECK_EQ_INT(clip.left, 0);
    CHECK_EQ_INT(clip.right, 639);
    CHECK_EQ_INT(clip.top, 20);
    CHECK_EQ_INT(clip.bottom, 40);
}
