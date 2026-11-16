#include "th01/palette.h"

#include "core/base/arena.h"

#include "test.h"

static void M_Init(
    ARENA *const arena, VIDEO *const video, PALETTE_STATE *const state)
{
    Arena_Init(arena, "palette test", 1024 * 1024);
    Video_Init(video, Arena_Allocator(arena));
    Palette_Init(state);
}

static PALETTE M_Colors(void)
{
    PALETTE colors;
    for (int32_t i = 0; i < 16; i++) {
        colors.colors[i][0] = (int8_t)i;
        colors.colors[i][1] = (int8_t)(15 - i);
        colors.colors[i][2] = (int8_t)((i * 7) % 16);
    }
    return colors;
}

static void M_CheckDac(
    const VIDEO *const video, const int32_t index, const int32_t red,
    const int32_t green, const int32_t blue)
{
    CHECK_EQ_INT(video->palette[index].red, (red & 15) * 17);
    CHECK_EQ_INT(video->palette[index].green, (green & 15) * 17);
    CHECK_EQ_INT(video->palette[index].blue, (blue & 15) * 17);
    CHECK_EQ_INT(video->palette[index].alpha, 255);
}

static void M_CheckShown(const VIDEO *const video, const PALETTE *const colors)
{
    for (int32_t i = 0; i < 16; i++) {
        M_CheckDac(
            video, i, colors->colors[i][0], colors->colors[i][1],
            colors->colors[i][2]);
    }
}

static int32_t M_Toned(const int32_t value, const int32_t tone)
{
    return tone <= 100 ? (value * tone + 99) / 100
                       : (value * (200 - tone) + 15 * (tone - 100)) / 100;
}

TEST(initial_state_has_independent_saved_resource_and_displayed_colors)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    static const PALETTE stock = { .colors = {
                                       { 0, 0, 0 },
                                       { 0, 0, 15 },
                                       { 0, 15, 0 },
                                       { 0, 15, 15 },
                                       { 15, 0, 0 },
                                       { 15, 0, 15 },
                                       { 15, 15, 0 },
                                       { 15, 15, 15 },
                                       { 8, 8, 8 },
                                       { 0, 0, 10 },
                                       { 0, 10, 0 },
                                       { 0, 10, 10 },
                                       { 10, 0, 0 },
                                       { 10, 0, 10 },
                                       { 10, 10, 0 },
                                       { 12, 12, 12 },
                                   } };
    const PALETTE empty = {};
    CHECK(memcmp(&state.saved, &stock, sizeof(stock)) == 0);
    CHECK(memcmp(&state.resource, &empty, sizeof(empty)) == 0);
    CHECK_EQ_INT(state.tone, 100);
    M_CheckShown(&video, &empty);
    Palette_Show(&state, &video);
    M_CheckShown(&video, &stock);
    Arena_Free(&arena);
}

TEST(saved_setters_clamp_while_hardware_only_writes_mask)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    for (int32_t value = -128; value <= 127; value++) {
        const PALETTE_STATE before = state;
        Palette_ShowSolid(&video, value, value + 1, value + 2);
        CHECK(memcmp(&state, &before, sizeof(state)) == 0);
        for (int32_t i = 0; i < 16; i++) {
            M_CheckDac(&video, i, value, value + 1, value + 2);
        }
        Palette_SetColor(&state, &video, 0, value, 15 - value, value);
        const int32_t expected = value < 0 ? 0 : value > 15 ? 15 : value;
        const int32_t inverse = value < 0 ? 15 : value > 15 ? 0 : 15 - value;
        M_CheckDac(&video, 0, expected, inverse, expected);
        CHECK_EQ_INT(state.saved.colors[0][0], expected);
        CHECK_EQ_INT(state.saved.colors[0][1], inverse);
        CHECK_EQ_INT(state.saved.colors[0][2], expected);
    }
    Palette_SetColor(&state, &video, 15, INT32_MIN, INT32_MAX, -1);
    M_CheckDac(&video, 15, 0, 15, 0);
    Palette_SetGray(&state, &video, INT32_MAX);
    for (int32_t i = 0; i < 16; i++) {
        M_CheckDac(&video, i, 15, 15, 15);
        CHECK_EQ_INT(state.saved.colors[i][0], 15);
    }
    Palette_SetGray(&state, &video, INT32_MIN);
    for (int32_t i = 0; i < 16; i++) {
        M_CheckDac(&video, i, 0, 0, 0);
    }
    Arena_Free(&arena);
}

TEST(set_accepts_saved_and_resource_aliases_without_mutating_resource)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    state.resource = M_Colors();
    state.resource.colors[0][0] = -1;
    state.resource.colors[15][1] = 127;
    const PALETTE resource = state.resource;
    Palette_Set(&state, &video, &state.resource);
    CHECK(memcmp(&state.resource, &resource, sizeof(resource)) == 0);
    CHECK_EQ_INT(state.saved.colors[0][0], 0);
    CHECK_EQ_INT(state.saved.colors[15][1], 15);
    state.saved.colors[1][0] = -128;
    state.saved.colors[2][1] = 16;
    Palette_Set(&state, &video, &state.saved);
    CHECK_EQ_INT(state.saved.colors[1][0], 0);
    CHECK_EQ_INT(state.saved.colors[2][1], 15);
    M_CheckShown(&video, &state.saved);
    Arena_Free(&arena);
}

TEST(resource_import_quantizes_channels_and_ignores_alpha)
{
    PALETTE colors;
    for (int32_t value = 0; value < 256; value++) {
        GFX_COLOR source[16];
        for (int32_t i = 0; i < 16; i++) {
            source[i] =
                (GFX_COLOR) { (uint8_t)value, (uint8_t)(255 - value),
                              (uint8_t)((value + 17) % 256), (uint8_t)i };
        }
        Palette_Import(&colors, source);
        for (int32_t i = 0; i < 16; i++) {
            CHECK_EQ_INT(colors.colors[i][0], value / 16);
            CHECK_EQ_INT(colors.colors[i][1], (255 - value) / 16);
            CHECK_EQ_INT(colors.colors[i][2], ((value + 17) % 256) / 16);
        }
    }
}

TEST(shared_fades_apply_all_sixteen_steps_without_changing_saved_colors)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    const PALETTE colors = M_Colors();
    for (PALETTE_FADE_KIND kind = PALETTE_FADE_BLACK_IN;
         kind <= PALETTE_FADE_WHITE_OUT; kind++) {
        Palette_Set(&state, &video, &colors);
        state.resource = colors;
        state.tone = 43;
        const PALETTE_STATE original = state;
        const VIDEO before = video;
        PALETTE_FADE fade;
        Palette_BeginFade(&fade, &state, kind);
        CHECK(
            memcmp(video.palette, before.palette, sizeof(video.palette)) == 0);
        for (int32_t step = 1; step <= 16; step++) {
            CHECK(Palette_FadeStep(&fade, &state, &video));
            CHECK(memcmp(&state, &original, sizeof(state)) == 0);
            PALETTE expected;
            for (int32_t i = 0; i < 16; i++) {
                for (int32_t c = 0; c < 3; c++) {
                    const int32_t value = colors.colors[i][c];
                    int32_t result;
                    switch (kind) {
                    case PALETTE_FADE_BLACK_IN:
                        result = value < step ? value : step;
                        break;
                    case PALETTE_FADE_BLACK_OUT:
                        result = value > step ? value - step : 0;
                        break;
                    case PALETTE_FADE_WHITE_IN:
                        result = value > 15 - step ? value : 15 - step;
                        break;
                    default:
                        result = value + step < 15 ? value + step : 15;
                        break;
                    }
                    expected.colors[i][c] = (int8_t)result;
                }
            }
            M_CheckShown(&video, &expected);
        }
        const VIDEO finished = video;
        CHECK(!Palette_FadeStep(&fade, &state, &video));
        CHECK(!Palette_FadeStep(&fade, &state, &video));
        CHECK(
            memcmp(video.palette, finished.palette, sizeof(video.palette))
            == 0);
    }
    Arena_Free(&arena);
}

TEST(shared_in_fades_use_live_directional_targets_but_out_starts_are_frozen)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    Palette_SetGray(&state, &video, 10);
    PALETTE_FADE fade;
    Palette_BeginFade(&fade, &state, PALETTE_FADE_BLACK_IN);
    for (int32_t i = 0; i < 5; i++) {
        CHECK(Palette_FadeStep(&fade, &state, &video));
    }
    Palette_SetGray(&state, &video, 2);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 5, 5, 5);
    Palette_SetGray(&state, &video, 9);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 6, 6, 6);
    Palette_BeginFade(&fade, &state, PALETTE_FADE_WHITE_IN);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 14, 14, 14);
    Palette_SetGray(&state, &video, 15);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 14, 14, 14);
    Palette_SetGray(&state, &video, 10);
    Palette_BeginFade(&fade, &state, PALETTE_FADE_BLACK_OUT);
    Palette_SetGray(&state, &video, 0);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 9, 9, 9);
    Palette_SetGray(&state, &video, 3);
    Palette_BeginFade(&fade, &state, PALETTE_FADE_WHITE_OUT);
    Palette_SetGray(&state, &video, 15);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 4, 4, 4);
    Arena_Free(&arena);
}

TEST(fade_from_keeps_initial_colors_then_tracks_live_saved_components)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    Palette_SetGray(&state, &video, 7);
    PALETTE_FADE fade;
    Palette_BeginFrom(&fade, &state, 0, 15, 7, 0x8001);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 7, 7, 7);
    M_CheckDac(&video, 15, 7, 7, 7);
    M_CheckDac(&video, 1, 1, 14, 7);
    Palette_SetGray(&state, &video, 9);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    M_CheckDac(&video, 0, 8, 8, 8);
    M_CheckDac(&video, 1, 2, 13, 8);
    for (int32_t i = 2; i < 16; i++) {
        CHECK(Palette_FadeStep(&fade, &state, &video));
    }
    M_CheckShown(&video, &state.saved);
    CHECK(!Palette_FadeStep(&fade, &state, &video));
    Arena_Free(&arena);
}

TEST(fade_from_preserves_signed_start_values_and_hardware_masking)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    Palette_SetGray(&state, &video, 7);
    PALETTE_FADE fade;
    Palette_BeginFrom(&fade, &state, INT8_MIN, INT8_MAX, -1, 0);
    for (int32_t step = 1; step <= 16; step++) {
        CHECK(Palette_FadeStep(&fade, &state, &video));
        M_CheckDac(
            &video, 0, INT8_MIN + step, INT8_MAX - step,
            step <= 8 ? -1 + step : 7);
        CHECK_EQ_INT(state.saved.colors[0][0], 7);
    }
    CHECK(!Palette_FadeStep(&fade, &state, &video));
    Arena_Free(&arena);
}

TEST(saved_steps_preserve_direction_and_alias_contracts)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    Palette_SetGray(&state, &video, 7);
    state.resource = M_Colors();
    const PALETTE resource = state.resource;
    Palette_StepTo(&state, &video, &state.resource, true);
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            CHECK_EQ_INT(
                state.saved.colors[i][c], resource.colors[i][c] > 7 ? 8 : 7);
        }
    }
    Palette_StepTo(&state, &video, &state.resource, false);
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            const int32_t target = resource.colors[i][c];
            CHECK_EQ_INT(
                state.saved.colors[i][c],
                target > 8        ? 9
                    : target > 7  ? 8
                    : target == 7 ? 7
                                  : 6);
        }
    }
    const PALETTE saved = state.saved;
    Palette_StepTo(&state, &video, &state.saved, false);
    CHECK(memcmp(&state.saved, &saved, sizeof(saved)) == 0);
    CHECK(memcmp(&state.resource, &resource, sizeof(resource)) == 0);
    M_CheckShown(&video, &saved);
    const PALETTE black = {};
    for (int32_t step = 0; step < 16; step++) {
        Palette_StepTo(&state, &video, &black, false);
    }
    CHECK(memcmp(&state.saved, &black, sizeof(black)) == 0);
    Arena_Free(&arena);
}

TEST(ending_tones_use_integer_blends_and_preserve_color_zero)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    state.resource = M_Colors();
    const PALETTE resource = state.resource;
    Palette_SetColor(&state, &video, 0, 3, 8, 14);
    for (int32_t tone = 0; tone <= 200; tone++) {
        Palette_SetTone(&state, &video, tone);
        CHECK_EQ_INT(state.tone, tone);
        M_CheckDac(&video, 0, 3, 8, 14);
        for (int32_t i = 1; i < 16; i++) {
            for (int32_t c = 0; c < 3; c++) {
                CHECK_EQ_INT(
                    state.saved.colors[i][c],
                    M_Toned(resource.colors[i][c], tone));
            }
        }
        M_CheckShown(&video, &state.saved);
        CHECK(memcmp(&state.resource, &resource, sizeof(resource)) == 0);
    }
    Palette_SetTone(&state, &video, INT32_MIN);
    CHECK_EQ_INT(state.tone, 0);
    M_CheckDac(&video, 1, 0, 0, 0);
    Palette_SetTone(&state, &video, INT32_MAX);
    CHECK_EQ_INT(state.tone, 200);
    M_CheckDac(&video, 1, 15, 15, 15);
    M_CheckDac(&video, 0, 3, 8, 14);
    Arena_Free(&arena);
}

TEST(resource_display_uses_the_persistent_tone_only_when_requested)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    state.resource = M_Colors();
    Palette_SetColor(&state, &video, 0, 4, 5, 6);
    Palette_SetTone(&state, &video, 47);
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            state.resource.colors[i][c] =
                (int8_t)(15 - state.resource.colors[i][c]);
        }
    }
    Palette_ShowResource(&state, &video, true);
    CHECK_EQ_INT(state.tone, 47);
    M_CheckDac(&video, 0, 4, 5, 6);
    for (int32_t i = 1; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            CHECK_EQ_INT(
                state.saved.colors[i][c],
                M_Toned(state.resource.colors[i][c], 47));
        }
    }
    Palette_ShowResource(&state, &video, false);
    CHECK_EQ_INT(state.tone, 47);
    CHECK(memcmp(&state.saved, &state.resource, sizeof(state.saved)) == 0);
    M_CheckShown(&video, &state.resource);
    Arena_Free(&arena);
}

TEST(ending_fades_take_twenty_steps_from_fixed_endpoints)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    state.resource = M_Colors();
    for (PALETTE_FADE_KIND kind = PALETTE_FADE_BLACK_IN;
         kind <= PALETTE_FADE_WHITE_OUT; kind++) {
        state.tone = 27;
        Palette_SetColor(&state, &video, 0, 2, 9, 13);
        PALETTE_FADE fade;
        const VIDEO before = video;
        Palette_BeginToneFade(&fade, kind);
        CHECK_EQ_INT(state.tone, 27);
        CHECK(
            memcmp(video.palette, before.palette, sizeof(video.palette)) == 0);
        for (int32_t step = 1; step <= 20; step++) {
            CHECK(Palette_FadeStep(&fade, &state, &video));
            const int32_t expected = kind == PALETTE_FADE_BLACK_IN ? step * 5
                : kind == PALETTE_FADE_BLACK_OUT ? 100 - step * 5
                : kind == PALETTE_FADE_WHITE_IN  ? 200 - step * 5
                                                 : 100 + step * 5;
            CHECK_EQ_INT(state.tone, expected);
            M_CheckDac(&video, 0, 2, 9, 13);
            for (int32_t i = 1; i < 16; i++) {
                for (int32_t c = 0; c < 3; c++) {
                    CHECK_EQ_INT(
                        state.saved.colors[i][c],
                        M_Toned(state.resource.colors[i][c], expected));
                }
            }
        }
        const PALETTE_STATE finished = state;
        CHECK(!Palette_FadeStep(&fade, &state, &video));
        CHECK(memcmp(&state, &finished, sizeof(state)) == 0);
    }
    Arena_Free(&arena);
}

TEST(ending_fades_read_resource_changes_at_each_step)
{
    ARENA arena;
    VIDEO video;
    PALETTE_STATE state;
    M_Init(&arena, &video, &state);
    state.resource = M_Colors();
    PALETTE_FADE fade;
    Palette_BeginToneFade(&fade, PALETTE_FADE_BLACK_IN);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    CHECK_EQ_INT(state.tone, 5);
    state.resource.colors[1][0] = 15;
    Palette_SetTone(&state, &video, 180);
    CHECK(Palette_FadeStep(&fade, &state, &video));
    CHECK_EQ_INT(state.tone, 10);
    CHECK_EQ_INT(state.saved.colors[1][0], 2);
    Arena_Free(&arena);
}
