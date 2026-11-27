#include "tests/test.h"
#include "th01/palette.h"

#include "core/base/arena.h"

static void M_Init(ARENA *const arena, PALETTE_STATE *const state)
{
    Arena_Init(arena, "palette test", 1024 * 1024);
    Video_Init(Arena_Allocator(arena));
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
    const int32_t index, const int32_t red, const int32_t green,
    const int32_t blue)
{
    CHECK_EQ_INT(g_Video.palette[index].red, (red & 15) * 17);
    CHECK_EQ_INT(g_Video.palette[index].green, (green & 15) * 17);
    CHECK_EQ_INT(g_Video.palette[index].blue, (blue & 15) * 17);
    CHECK_EQ_INT(g_Video.palette[index].alpha, 255);
}

static void M_CheckShown(const PALETTE *const colors)
{
    for (int32_t i = 0; i < 16; i++) {
        M_CheckDac(
            i, colors->colors[i][0], colors->colors[i][1],
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
    PALETTE_STATE state;
    M_Init(&arena, &state);
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
    M_CheckShown(&empty);
    Palette_Set(&state, &state.saved);
    M_CheckShown(&stock);
    Arena_Free(&arena);
}

TEST(saved_setters_clamp_while_hardware_only_writes_mask)
{
    ARENA arena;
    PALETTE_STATE state;
    M_Init(&arena, &state);
    for (int32_t value = -128; value <= 127; value++) {
        const PALETTE_STATE before = state;
        Palette_ShowSolid(value, value + 1, value + 2);
        CHECK(memcmp(&state, &before, sizeof(state)) == 0);
        for (int32_t i = 0; i < 16; i++) {
            M_CheckDac(i, value, value + 1, value + 2);
        }
        Palette_SetColor(&state, 0, value, 15 - value, value);
        const int32_t expected = value < 0 ? 0 : value > 15 ? 15 : value;
        const int32_t inverse = value < 0 ? 15 : value > 15 ? 0 : 15 - value;
        M_CheckDac(0, expected, inverse, expected);
        CHECK_EQ_INT(state.saved.colors[0][0], expected);
        CHECK_EQ_INT(state.saved.colors[0][1], inverse);
        CHECK_EQ_INT(state.saved.colors[0][2], expected);
    }
    Palette_SetColor(&state, 15, INT32_MIN, INT32_MAX, -1);
    M_CheckDac(15, 0, 15, 0);
    Palette_SetGray(&state, INT32_MAX);
    for (int32_t i = 0; i < 16; i++) {
        M_CheckDac(i, 15, 15, 15);
        CHECK_EQ_INT(state.saved.colors[i][0], 15);
    }
    Palette_SetGray(&state, INT32_MIN);
    for (int32_t i = 0; i < 16; i++) {
        M_CheckDac(i, 0, 0, 0);
    }
    Arena_Free(&arena);
}

TEST(set_accepts_saved_and_resource_aliases_without_mutating_resource)
{
    ARENA arena;
    PALETTE_STATE state;
    M_Init(&arena, &state);
    state.resource = M_Colors();
    state.resource.colors[0][0] = -1;
    state.resource.colors[15][1] = 127;
    const PALETTE resource = state.resource;
    Palette_Set(&state, &state.resource);
    CHECK(memcmp(&state.resource, &resource, sizeof(resource)) == 0);
    CHECK_EQ_INT(state.saved.colors[0][0], 0);
    CHECK_EQ_INT(state.saved.colors[15][1], 15);
    state.saved.colors[1][0] = -128;
    state.saved.colors[2][1] = 16;
    Palette_Set(&state, &state.saved);
    CHECK_EQ_INT(state.saved.colors[1][0], 0);
    CHECK_EQ_INT(state.saved.colors[2][1], 15);
    M_CheckShown(&state.saved);
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
    PALETTE_STATE state;
    M_Init(&arena, &state);
    const PALETTE colors = M_Colors();
    for (PALETTE_FADE_KIND kind = PALETTE_FADE_BLACK_IN;
         kind <= PALETTE_FADE_WHITE_OUT; kind++) {
        Palette_Set(&state, &colors);
        state.resource = colors;
        state.tone = 43;
        const PALETTE_STATE original = state;
        const VIDEO before = g_Video;
        PALETTE_FADE fade;
        Palette_BeginFade(&fade, &state, kind);
        CHECK(
            memcmp(g_Video.palette, before.palette, sizeof(g_Video.palette))
            == 0);
        for (int32_t step = 1; step <= 16; step++) {
            CHECK(Palette_FadeStep(&fade, &state));
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
            M_CheckShown(&expected);
        }
        const VIDEO finished = g_Video;
        CHECK(!Palette_FadeStep(&fade, &state));
        CHECK(!Palette_FadeStep(&fade, &state));
        CHECK(
            memcmp(g_Video.palette, finished.palette, sizeof(g_Video.palette))
            == 0);
    }
    Arena_Free(&arena);
}

TEST(shared_in_fades_use_live_directional_targets_but_out_starts_are_frozen)
{
    ARENA arena;
    PALETTE_STATE state;
    M_Init(&arena, &state);
    Palette_SetGray(&state, 10);
    PALETTE_FADE fade;
    Palette_BeginFade(&fade, &state, PALETTE_FADE_BLACK_IN);
    for (int32_t i = 0; i < 5; i++) {
        CHECK(Palette_FadeStep(&fade, &state));
    }
    Palette_SetGray(&state, 2);
    CHECK(Palette_FadeStep(&fade, &state));
    M_CheckDac(0, 5, 5, 5);
    Palette_SetGray(&state, 9);
    CHECK(Palette_FadeStep(&fade, &state));
    M_CheckDac(0, 6, 6, 6);
    Palette_BeginFade(&fade, &state, PALETTE_FADE_WHITE_IN);
    CHECK(Palette_FadeStep(&fade, &state));
    M_CheckDac(0, 14, 14, 14);
    Palette_SetGray(&state, 15);
    CHECK(Palette_FadeStep(&fade, &state));
    M_CheckDac(0, 14, 14, 14);
    Palette_SetGray(&state, 10);
    Palette_BeginFade(&fade, &state, PALETTE_FADE_BLACK_OUT);
    Palette_SetGray(&state, 0);
    CHECK(Palette_FadeStep(&fade, &state));
    M_CheckDac(0, 9, 9, 9);
    Palette_SetGray(&state, 3);
    Palette_BeginFade(&fade, &state, PALETTE_FADE_WHITE_OUT);
    Palette_SetGray(&state, 15);
    CHECK(Palette_FadeStep(&fade, &state));
    M_CheckDac(0, 4, 4, 4);
    Arena_Free(&arena);
}

TEST(saved_steps_preserve_direction_and_alias_contracts)
{
    ARENA arena;
    PALETTE_STATE state;
    M_Init(&arena, &state);
    Palette_SetGray(&state, 7);
    state.resource = M_Colors();
    const PALETTE resource = state.resource;
    Palette_StepTo(&state, &state.resource, true);
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            CHECK_EQ_INT(
                state.saved.colors[i][c], resource.colors[i][c] > 7 ? 8 : 7);
        }
    }
    Palette_StepTo(&state, &state.resource, false);
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
    Palette_StepTo(&state, &state.saved, false);
    CHECK(memcmp(&state.saved, &saved, sizeof(saved)) == 0);
    CHECK(memcmp(&state.resource, &resource, sizeof(resource)) == 0);
    M_CheckShown(&saved);
    const PALETTE black = {};
    for (int32_t step = 0; step < 16; step++) {
        Palette_StepTo(&state, &black, false);
    }
    CHECK(memcmp(&state.saved, &black, sizeof(black)) == 0);
    Arena_Free(&arena);
}

TEST(ending_tones_use_integer_blends_and_preserve_color_zero)
{
    ARENA arena;
    PALETTE_STATE state;
    M_Init(&arena, &state);
    state.resource = M_Colors();
    const PALETTE resource = state.resource;
    Palette_SetColor(&state, 0, 3, 8, 14);
    for (int32_t tone = 0; tone <= 200; tone++) {
        Palette_SetTone(&state, tone);
        CHECK_EQ_INT(state.tone, tone);
        M_CheckDac(0, 3, 8, 14);
        for (int32_t i = 1; i < 16; i++) {
            for (int32_t c = 0; c < 3; c++) {
                CHECK_EQ_INT(
                    state.saved.colors[i][c],
                    M_Toned(resource.colors[i][c], tone));
            }
        }
        M_CheckShown(&state.saved);
        CHECK(memcmp(&state.resource, &resource, sizeof(resource)) == 0);
    }
    Palette_SetTone(&state, INT32_MIN);
    CHECK_EQ_INT(state.tone, 0);
    M_CheckDac(1, 0, 0, 0);
    Palette_SetTone(&state, INT32_MAX);
    CHECK_EQ_INT(state.tone, 200);
    M_CheckDac(1, 15, 15, 15);
    M_CheckDac(0, 3, 8, 14);
    Arena_Free(&arena);
}

TEST(resource_display_uses_the_persistent_tone_only_when_requested)
{
    ARENA arena;
    PALETTE_STATE state;
    M_Init(&arena, &state);
    state.resource = M_Colors();
    Palette_SetColor(&state, 0, 4, 5, 6);
    Palette_SetTone(&state, 47);
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            state.resource.colors[i][c] =
                (int8_t)(15 - state.resource.colors[i][c]);
        }
    }
    Palette_ShowResource(&state, true);
    CHECK_EQ_INT(state.tone, 47);
    M_CheckDac(0, 4, 5, 6);
    for (int32_t i = 1; i < 16; i++) {
        for (int32_t c = 0; c < 3; c++) {
            CHECK_EQ_INT(
                state.saved.colors[i][c],
                M_Toned(state.resource.colors[i][c], 47));
        }
    }
    Palette_ShowResource(&state, false);
    CHECK_EQ_INT(state.tone, 47);
    CHECK(memcmp(&state.saved, &state.resource, sizeof(state.saved)) == 0);
    M_CheckShown(&state.resource);
    Arena_Free(&arena);
}
