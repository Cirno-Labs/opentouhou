#include "tests/test.h"
#include "tests/th01/th01_text_fixture.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/boss/boss.c"
#include "th01/boss/private.h"
#include "th01/orb.c"
#include "th01/player.c"
#include "th01/session.h"
#include "th01/shots.c"
#include "th01/stage/private.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PALETTE baseline;
    BOSS_HIT hit;
    PTN sprites;
    GAME_TEXT_FONT font;
    int16_t hp;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    *f = (FIXTURE) { .hp = 8 };
    g_Session.lives = 3;
    Arena_Init(&f->arena, "boss hit", 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&f->arena);
    Video_Init(a);
    Hud_Init(a);
    HudHp_Init(a);
    HudHp_Rerender(8);
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
    Palette_Set(p_Palette, &f->baseline);
    Hud_Load(nullptr, &f->sprites, &f->font);
}

TEST(real_hud_health_and_flash_follow_hit_order)
{
    Th01Test_ResetWorld();
    FIXTURE f;
    M_Init(&f);
    const uint8_t colors[] = { 2 };
    p_Stage->score = UINT32_MAX - 5;
    m_Orb.left = 303;
    BossHit_UpdateAndRender(
        &f.hit, &f.hp, &f.baseline, colors, 1, 10, true, (BOSS_SHOT_HITBOX) {});
    CHECK_EQ_INT(g_Stage->score, 4);
    CHECK_EQ_INT(g_Session.high_score, 4);
    CHECK_EQ_INT(m_Orb.velocity_x, ORB_X_RIGHT_4);
    CHECK_EQ_INT(f.hp, 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 184, 48), 0);
    CHECK_EQ_INT(g_Palette->saved.colors[2][0], 1);
    CHECK_EQ_INT(g_Video.palette[2].red, 17);
    Arena_Free(&f.arena);
}

TEST(debug_damage_precedes_shot_collision_while_invincible)
{
    Th01Test_ResetWorld();
    FIXTURE f;
    M_Init(&f);
    m_TestDamage = true;
    f.hit = (BOSS_HIT) { .frame = 1, .invincible = true };
    m_Shots.slots[0] = (SHOT) { .left = 120, .top = 100, .moving = true };
    BossHit_UpdateAndRender(
        &f.hit, &f.hp, &f.baseline, nullptr, 0, 3000, true,
        (BOSS_SHOT_HITBOX) { 100, 100, 40, 40 });
    CHECK(!m_TestDamage);
    CHECK_EQ_INT(f.hp, 7);
    CHECK_EQ_INT(g_Stage->score, 0);
    CHECK(!m_Shots.slots[0].moving);
    CHECK_EQ_INT(m_Shots.slots[0].decay_frame, 1);
    CHECK_EQ_INT(f.hit.frame, 1);
    Arena_Free(&f.arena);
}

TEST(bomb_damage_ignores_invincibility_on_its_real_cadence)
{
    Th01Test_ResetWorld();
    FIXTURE f;
    M_Init(&f);
    const uint8_t colors[] = { 2 };
    g_Session.lives = 0;
    m_Player.bomb_damaging = true;
    f.hit.invincible = true;
    g_Session.frame_rand = 49;
    BossHit_UpdateAndRender(
        &f.hit, &f.hp, &f.baseline, colors, 1, -1, false,
        (BOSS_SHOT_HITBOX) {});
    CHECK_EQ_INT(f.hp, 8);
    g_Session.frame_rand = 50;
    BossHit_UpdateAndRender(
        &f.hit, &f.hp, &f.baseline, colors, 1, -1, false,
        (BOSS_SHOT_HITBOX) {});
    CHECK_EQ_INT(f.hp, 7);
    CHECK_EQ_INT(g_Stage->score, UINT32_MAX);
    Arena_Free(&f.arena);
}

TEST(palette_reset_is_strictly_after_forty_even_without_invincibility)
{
    Th01Test_ResetWorld();
    FIXTURE f;
    M_Init(&f);
    const uint8_t colors[] = { 2 };
    f.hit = (BOSS_HIT) { .frame = 40, .invincible = true };
    Palette_SetColor(p_Palette, 2, 15, 15, 15);
    BossHit_UpdateAndRender(
        &f.hit, &f.hp, &f.baseline, colors, 1, 3000, false,
        (BOSS_SHOT_HITBOX) {});
    CHECK(f.hit.invincible);
    CHECK_EQ_INT(f.hit.frame, 40);
    CHECK_EQ_INT(g_Palette->saved.colors[2][0], 15);
    f.hit = (BOSS_HIT) { .frame = 41 };
    BossHit_UpdateAndRender(
        &f.hit, &f.hp, &f.baseline, colors, 1, 3000, false,
        (BOSS_SHOT_HITBOX) {});
    CHECK(!f.hit.invincible);
    CHECK_EQ_INT(f.hit.frame, 0);
    CHECK_EQ_INT(g_Palette->saved.colors[2][0], 1);
    Arena_Free(&f.arena);
}
