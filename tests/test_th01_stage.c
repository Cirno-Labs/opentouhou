#include "th01/stage.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"

#include <string.h>

#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

typedef struct {
    ARENA arena;
    STAGE_CONTEXT context;
    STAGE_OBJECTS objects;
    STAGE_GAMEPLAY game;
    PTN sprites;
    GAME_TEXT_FONT font;
    int32_t events[1024], count;
    int32_t points, bombs, pellets;
    STAGE_PELLET_SPAWN last_pellet;
} CASE;

static void M_Hud(void *const user)
{
    CASE *const state = user;
    CHECK(state->game.score >= state->objects.cards[0].score);
    state->events[state->count++] = 1;
}

static void M_Item(void *const user, const int32_t slot, const bool bomb)
{
    CASE *const state = user;
    CHECK_EQ_INT(slot, 0);
    CHECK_EQ_INT(state->events[state->count - 1], 1);
    state->events[state->count++] = bomb ? 3 : 2;
    state->bombs += bomb;
    state->points += !bomb;
}

static void M_Effect(void *const user, const int16_t number)
{
    CASE *const state = user;
    CHECK_EQ_INT(number, 2);
    state->events[state->count++] = 4;
}

static void M_Pellet(void *const user, const STAGE_PELLET_SPAWN spawn)
{
    CASE *const state = user;
    state->pellets++;
    state->last_pellet = spawn;
    CHECK_EQ_INT(state->objects.cards[0].flip_frame, 25);
    CHECK_EQ_INT(state->objects.cards[0].flag, STAGE_CARD_FLIPPING);
    MasterRng_Next(&g_Rng);
}

static void M_Init(CASE *const state, const uint8_t hp)
{
    *state = (CASE) {};
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    g_Rng = 0;
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.left = 96;
    g_Orb.top = 128;
    g_Rng = 1;
    Arena_Init(&state->arena, "stage", 1024 * 1024);
    Video_Init(Arena_Allocator(&state->arena));
    state->sprites =
        (PTN) { .count = 34, .pixels = Arena_Alloc(&state->arena, 34 * 1024) };
    memset(state->sprites.pixels, PTN_TRANSPARENT, 34 * 1024);
    for (int32_t i = 0; i < 34; i++) {
        state->sprites.pixels[i * 1024] = i % 15;
    }
    STAGE_DATA_STAGE layouts[5] = {};
    layouts[0].cells[2][3] =
        (STAGE_DATA_CELL) { .card = true, .card_extra_hp = hp };
    STAGE_DATA data = { .stages = layouts };
    StageObjects_Init(
        &state->objects, &data, 0, true, Arena_Allocator(&state->arena),
        &state->sprites);
    GAME_TEXT_GLYPH digits[10] = {};
    for (int32_t i = 0; i < 10; i++) {
        digits[i].codepoint = (uint32_t)('0' + i);
        digits[i].width = 8;
        digits[i].rows[0] = 0x8000;
    }
    CHECK(IS_OK(GameText_LoadGlyphs(
        digits, 10, Arena_Allocator(&state->arena), &state->font)));
    state->context = (STAGE_CONTEXT) {
        .objects = &state->objects,
        .gameplay = &state->game,
        .sprites = &state->sprites,
        .font = &state->font,
        .stage_num = 1,
        .difficulty = SETTINGS_DIFFICULTY_NORMAL,
        .hud_score_and_combo = M_Hud,
        .item_add = M_Item,
        .effect = M_Effect,
        .pellet_group = M_Pellet,
        .user = state,
    };
}

TEST(card_scores_caps_widths_and_synchronous_order)
{
    const int16_t combos[] = { 0, 1, 26, 27, 100, 181, 182, 256, 32767 };
    for (int32_t rank = 0; rank < 4; rank++) {
        for (int32_t variant = 0; variant < 9; variant++) {
            CASE state;
            M_Init(&state, 0);
            state.game.combo = combos[variant];
            state.context.stage_id = 19;
            state.context.difficulty = rank;
            const ORB orb = g_Orb;
            const uint32_t rng = g_Rng;
            Stage_HitTestCards(&state.context);
            const uint32_t square = (uint32_t)(int32_t)Math16_Wrap(
                (int32_t)combos[variant] * combos[variant]);
            uint32_t expected = 400 + square * (rank == 3 ? 35u : 20u);
            if (expected > 25600) {
                expected = 25600;
            }
            CHECK_EQ_INT(state.game.score, expected);
            CHECK_EQ_INT(state.game.combo, Math16_Wrap(combos[variant] + 1));
            CHECK_EQ_INT(
                state.game.maximum_combo,
                combos[variant] == 32767 ? 0 : combos[variant] + 1);
            CHECK_EQ_INT(state.objects.cards[0].flag, STAGE_CARD_FLIPPING);
            CHECK(memcmp(&orb, &g_Orb, sizeof(ORB)) == 0);
            CHECK_EQ_INT(g_Rng, rng);
            CHECK_EQ_INT(state.count, 3);
            CHECK_EQ_INT(state.events[0], 1);
            CHECK_EQ_INT(state.events[1], 2);
            CHECK_EQ_INT(state.events[2], 4);
            Arena_Free(&state.arena);
        }
    }
}

TEST(all_postfix_drop_cycles_and_bomb_short_circuit)
{
    for (int32_t cycle = 0; cycle < 256; cycle++) {
        for (int32_t damaging = 0; damaging < 2; damaging++) {
            CASE state;
            M_Init(&state, 0);
            g_Player.bomb_damaging = damaging;
            state.game.flip_cycle = (uint8_t)cycle;
            const uint32_t rng = g_Rng;
            Stage_HitTestCards(&state.context);
            const uint8_t next = (uint8_t)(cycle + 1);
            const bool drop = cycle % 10 == 0;
            const bool reset = drop && next >= 140;
            CHECK_EQ_INT(state.game.flip_cycle, reset ? 1 : next);
            CHECK_EQ_INT(state.bombs, reset && !damaging);
            CHECK_EQ_INT(state.points, drop && (!reset || damaging));
            CHECK_EQ_INT(g_Rng, rng);
            Arena_Free(&state.arena);
        }
    }
    for (int32_t flag = 0; flag < 3; flag++) {
        for (uint32_t seed = 1; seed < 12; seed++) {
            CASE state;
            M_Init(&state, 0);
            state.objects.cards[0].flag = flag;
            g_Player.bomb_damaging = true;
            g_Orb.left = 500;
            g_Rng = seed;
            uint32_t expected = seed;
            const bool selected = MasterRng_Next(&expected) % 4 != 0;
            Stage_HitTestCards(&state.context);
            CHECK_EQ_INT(g_Rng, expected);
            CHECK_EQ_INT(state.game.combo, selected && flag == 0);
            Arena_Free(&state.arena);
        }
    }
}

TEST(card_hp_frames_revenge_and_stage_exit)
{
    for (int32_t hp = 0; hp < 4; hp++) {
        for (int32_t stage = 9; stage <= 10; stage++) {
            CASE state;
            M_Init(&state, (uint8_t)hp);
            state.context.difficulty = SETTINGS_DIFFICULTY_LUNATIC;
            state.context.stage_num = (int16_t)stage;
            const size_t used = state.arena.used;
            uint32_t rng = g_Rng;
            for (int32_t flip = 0; flip <= hp; flip++) {
                Stage_HitTestCards(&state.context);
                for (int32_t frame = 0; frame < 30; frame++) {
                    Stage_UpdateCards(&state.context);
                    if (frame == 24) {
                        MasterRng_Next(&rng);
                    }
                    CHECK_EQ_INT(g_Rng, rng);
                    CHECK_EQ_INT(state.pellets, flip + (frame >= 24));
                    CHECK_EQ_INT(
                        state.objects.cards[0].flip_frame,
                        frame == 29 ? 0 : frame + 1);
                    CHECK_EQ_INT(
                        state.objects.cards[0].hp,
                        hp - flip - (frame == 29 && hp > flip));
                    CHECK_EQ_INT(state.game.cleared, flip == hp && frame == 29);
                    CHECK_EQ_INT(g_Player.is_hit, state.game.cleared);
                    CHECK_EQ_INT(state.arena.used, used);
                }
                CHECK_EQ_INT(state.last_pellet.left, 108);
                CHECK_EQ_INT(state.last_pellet.top, 140);
                CHECK_EQ_INT(state.last_pellet.speed, 56);
                CHECK_EQ_INT(state.last_pellet.group, stage == 9 ? 12 : 29);
            }
            Arena_Free(&state.arena);
        }
    }
}

TEST(score_popups_render_before_card_animation_and_clear_at_28)
{
    CASE state;
    M_Init(&state, 0);
    CHECK(IS_OK(Stage_ValidateScoreFont(&state.font)));
    const uint32_t scores[] = { 100, 1000, 1001, 10000, 10001, 25600 };
    for (int32_t i = 0; i < 6; i++) {
        STAGE_CARD *const card = &state.objects.cards[0];
        card->flag = STAGE_CARD_ALIVE;
        card->score = scores[i];
        card->flip_frame = 15;
        memset(g_Video.pages[0], 4, VIDEO_PAGE_SIZE);
        Stage_UpdateCards(&state.context);
        const int16_t x = card->left
            + (scores[i] > 10000      ? -8
                   : scores[i] > 1000 ? 0
                                      : 8);
        CHECK_EQ_INT(Th01Test_Pixel(0, x, card->top + 8), 7);
        card->flip_frame = 28;
        Stage_UpdateCards(&state.context);
        CHECK_EQ_INT(card->score, 0);
        CHECK_EQ_INT(g_Video.drawing_page, 0);
    }
    Arena_Free(&state.arena);
}

TEST(card_contact_bounds_and_portal_do_not_change_orb)
{
    const int16_t deltas[] = { -24, -23, 0, 23, 24 };
    for (int32_t y = 0; y < 5; y++) {
        for (int32_t x = 0; x < 5; x++) {
            CASE state;
            M_Init(&state, 0);
            g_Orb.left += deltas[x];
            g_Orb.top += deltas[y];
            g_Orb.in_portal = true;
            const ORB orb = g_Orb;
            Stage_HitTestCards(&state.context);
            CHECK_EQ_INT(state.game.combo, x > 0 && x < 4 && y > 0 && y < 4);
            CHECK(memcmp(&orb, &g_Orb, sizeof(ORB)) == 0);
            Arena_Free(&state.arena);
        }
    }
}

TEST(card_animation_overwrites_the_preceding_popup)
{
    CASE state;
    M_Init(&state, 0);
    STAGE_CARD *const card = &state.objects.cards[0];
    card->flag = STAGE_CARD_FLIPPING;
    card->flip_frame = 18;
    card->score = 1000;
    memset(state.sprites.pixels + 20 * 1024, 3, 1024);
    Stage_UpdateCards(&state.context);
    CHECK_EQ_INT(Th01Test_Pixel(0, 104, 136), 3);
    CHECK_EQ_INT(Th01Test_Pixel(1, 104, 136), 3);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&state.arena);
}
