#include "th01/boss_hit.h"

#include "core/base/arena.h"

#include "test.h"

#include <string.h>

typedef struct {
    ARENA arena;
    VIDEO video;
    PALETTE_STATE palette;
    PALETTE baseline;
    HUD hud;
    HUD_HP health;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT context;
    BOSS_HIT hit;
    STAGE_GAMEPLAY gameplay;
    PLAYER player;
    ORB orb;
    SHOTS shots;
    PTN sprites;
    GAME_TEXT_FONT font;
    uint32_t high_score;
    int16_t lives, hp, expected_shot_hp;
    bool test_damage;
    int32_t events[8], count;
} FIXTURE;

static void M_Shot(void *const user, const SHOTS_ACTION action)
{
    FIXTURE *const f = user;
    CHECK_EQ_INT(f->hp, f->expected_shot_hp);
    if (action.kind == SHOTS_ACTION_EFFECT) {
        CHECK_EQ_INT(action.effect, 16);
        CHECK(!f->shots.slots[0].moving);
        CHECK_EQ_INT(f->shots.slots[0].decay_frame, 1);
        f->events[f->count++] = 16;
    } else {
        Shots_RenderAction(&f->video, &f->sprites, action);
        f->events[f->count++] = 1;
    }
}

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK_EQ_INT(number, 3);
    CHECK_EQ_INT(f->hp, 7);
    CHECK(f->hit.invincible);
    CHECK_EQ_INT(f->hit.frame, 0);
    CHECK_EQ_INT(f->hud.score_previous, f->gameplay.score);
    CHECK_EQ_INT(f->palette.saved.colors[2][0], 15);
    CHECK_EQ_INT(Video_ReadPixel(&f->video, 0, 184, 48), 7);
    f->events[f->count++] = number;
}

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) { .lives = 3, .hp = 8, .expected_shot_hp = 8 };
    Arena_Init(&f->arena, "boss hit", 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(&f->video, a);
    Hud_Init(&f->hud, a);
    HudHp_Init(&f->health, a);
    HudHp_Rerender(&f->health, &f->video, 8);
    GAME_TEXT_GLYPH glyphs[10] = {};
    for (int32_t i = 0; i < 10; i++) {
        glyphs[i].codepoint = 0xff10 + i;
        glyphs[i].width = 16;
        glyphs[i].rows[0] = 0x8000u >> i;
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 10, a, &f->font)));
    f->sprites = (PTN) { .count = 2, .pixels = Allocator_Alloc(a, 2048) };
    memset(f->sprites.pixels, 15, 2048);
    f->baseline.colors[2][0] = 1;
    f->baseline.colors[2][1] = 2;
    f->baseline.colors[2][2] = 3;
    Palette_Set(&f->palette, &f->video, &f->baseline);
    f->display = (HUD_CONTEXT) { .video = &f->video,
                                 .sprites = &f->sprites,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives };
    f->context = (BOSS_HIT_CONTEXT) { .palette = &f->palette,
                                      .boss_palette = &f->baseline,
                                      .hud = &f->hud,
                                      .display = &f->display,
                                      .health = &f->health,
                                      .shots = &f->shots,
                                      .shots_output = { M_Shot, f },
                                      .orb = &f->orb,
                                      .hp = &f->hp,
                                      .test_damage = &f->test_damage,
                                      .effect = M_Effect,
                                      .user = f };
}

TEST(real_hud_health_and_flash_follow_hit_order)
{
    FIXTURE f;
    M_Init(&f);
    const uint8_t colors[] = { 2 };
    f.gameplay.score = UINT32_MAX - 5;
    f.orb.left = 303;
    BossHit_UpdateAndRender(
        &f.hit, &f.context, colors, 1, 10, true, (BOSS_SHOT_HITBOX) {});
    CHECK_EQ_INT(f.gameplay.score, 4);
    CHECK_EQ_INT(f.high_score, 4);
    CHECK_EQ_INT(f.orb.velocity_x, ORB_X_RIGHT_4);
    CHECK_EQ_INT(f.hp, 7);
    CHECK_EQ_INT(f.count, 1);
    CHECK_EQ_INT(f.events[0], 3);
    CHECK_EQ_INT(Video_ReadPixel(&f.video, 0, 184, 48), 0);
    CHECK_EQ_INT(f.palette.saved.colors[2][0], 1);
    CHECK_EQ_INT(f.video.palette[2].red, 17);
    Arena_Free(&f.arena);
}

TEST(debug_damage_precedes_shot_collision_while_invincible)
{
    FIXTURE f;
    M_Init(&f);
    f.test_damage = true;
    f.expected_shot_hp = 7;
    f.hit = (BOSS_HIT) { .frame = 1, .invincible = true };
    f.shots.slots[0] = (SHOT) { .left = 120, .top = 100, .moving = true };
    BossHit_UpdateAndRender(
        &f.hit, &f.context, nullptr, 0, 3000, true,
        (BOSS_SHOT_HITBOX) { 100, 100, 40, 40 });
    CHECK(!f.test_damage);
    CHECK_EQ_INT(f.hp, 7);
    CHECK_EQ_INT(f.gameplay.score, 0);
    CHECK_EQ_INT(f.count, 2);
    CHECK_EQ_INT(f.events[0], 1);
    CHECK_EQ_INT(f.events[1], 16);
    CHECK_EQ_INT(f.hit.frame, 1);
    Arena_Free(&f.arena);
}

TEST(bomb_damage_ignores_invincibility_on_its_real_cadence)
{
    FIXTURE f;
    M_Init(&f);
    const uint8_t colors[] = { 2 };
    f.lives = 0;
    f.player.bomb_damaging = true;
    f.hit.invincible = true;
    f.context.frame_rand = 49;
    BossHit_UpdateAndRender(
        &f.hit, &f.context, colors, 1, -1, false, (BOSS_SHOT_HITBOX) {});
    CHECK_EQ_INT(f.hp, 8);
    f.context.frame_rand = 50;
    BossHit_UpdateAndRender(
        &f.hit, &f.context, colors, 1, -1, false, (BOSS_SHOT_HITBOX) {});
    CHECK_EQ_INT(f.hp, 7);
    CHECK_EQ_INT(f.gameplay.score, UINT32_MAX);
    Arena_Free(&f.arena);
}

TEST(palette_reset_is_strictly_after_forty_even_without_invincibility)
{
    FIXTURE f;
    M_Init(&f);
    const uint8_t colors[] = { 2 };
    f.hit = (BOSS_HIT) { .frame = 40, .invincible = true };
    Palette_SetColor(&f.palette, &f.video, 2, 15, 15, 15);
    BossHit_UpdateAndRender(
        &f.hit, &f.context, colors, 1, 3000, false, (BOSS_SHOT_HITBOX) {});
    CHECK(f.hit.invincible);
    CHECK_EQ_INT(f.hit.frame, 40);
    CHECK_EQ_INT(f.palette.saved.colors[2][0], 15);
    f.hit = (BOSS_HIT) { .frame = 41 };
    BossHit_UpdateAndRender(
        &f.hit, &f.context, colors, 1, 3000, false, (BOSS_SHOT_HITBOX) {});
    CHECK(!f.hit.invincible);
    CHECK_EQ_INT(f.hit.frame, 0);
    CHECK_EQ_INT(f.palette.saved.colors[2][0], 1);
    Arena_Free(&f.arena);
}
