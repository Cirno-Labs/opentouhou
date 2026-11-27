#include "th01/shape.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

#include <string.h>

TEST(arc_cache_boundaries_and_defined_first_restore)
{
    ARENA arena;
    Arena_Init(&arena, "shape", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Shape_EllipseArcPut(100, 100, 16, 16, 7, 64, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 116, 100), 0);
    Shape_EllipseArcPut(100, 100, 16, 16, 7, 64, 0, 64);
    CHECK_EQ_INT(Th01Test_Pixel(0, 116, 100), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 100, 116), 0);
    Shape_EllipseArcPut(-8, 20, 8, 8, 7, 128, 0, 128);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 20), 0);
    memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
    Shape_EllipseArcErase(103, 100, 0, 0, 8, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 96, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 111, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 112, 100), 0);
    Arena_Free(&arena);
}
