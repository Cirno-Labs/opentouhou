#include "th01/boss_b05.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "th01/boss.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"
#include "th01/sprite.h"

#include "core/base/assert.h"

#include <string.h>

B05 g_B05;

typedef enum {
    B05_FIRE_NONE,
    B05_FIRE_DOWN,
    B05_FIRE_SLING,
} B05_FIRE;

static void M_Put(const BOSS_ENTITY *const entity, const int16_t image)
{
    BossEntity_RestorePut(entity, g_B05.sphere.left, g_B05.sphere.top, image);
}

static void M_Rotate(const int16_t interval, const int16_t delta)
{
    if (g_B05.frame % interval != 0) {
        return;
    }
    const uint8_t bits = (uint8_t)(g_B05.sphere.image + delta);
    int16_t image = bits < 128 ? bits : (int16_t)bits - 256;
    if (image > 7) {
        image = 0;
    } else if (image < 0) {
        image = 7;
    }
    g_B05.sphere.image = image;
    M_Put(&g_B05.sphere, image);
}

static void M_Accelerate(const int16_t delta)
{
    if (g_B05.frame < 50) {
        g_B05.sphere.image = 0;
        if (g_B05.frame % 4 == 0) {
            M_Put(&g_B05.sphere, 0);
        }
        return;
    }
    if (g_B05.frame == 50) {
        Sound_Effect(8);
    }
    if (g_B05.frame < 100 && g_B05.frame % 4 == 0) {
        M_Put(&g_B05.sphere, g_B05.sphere.image);
        if (g_B05.frame == 60 || g_B05.frame == 68 || g_B05.frame > 82) {
            M_Rotate(1, delta);
        }
    }
}

static void M_Move(const int16_t dx, const int16_t dy)
{
    BOSS_ENTITY *const e = &g_B05.sphere;
    if (dy < 0) {
        Sprite_Restore16(
            e->left, Math16_Wrap(e->top + dy + 96), 96, Math16_Wrap(-dy));
    } else if (dy > 0) {
        Sprite_Restore16(e->left, e->top, 96, dy);
    }
    if (dx > 0) {
        Sprite_Restore16(e->left, e->top, 8, 96);
    } else if (dx < 0) {
        Sprite_Restore16(Math16_Wrap(e->left + 96 - dy), e->top, 8, 96);
    }
    e->top = Math16_Wrap(e->top + dy);
    e->left = Math16_Wrap(e->left + dx);
    if (e->left < 0) {
        e->left = 0;
    }
    if (e->left > 544) {
        e->left = 544;
    }
    if (g_B05.frame % 2 == 0) {
        M_Rotate(1, 1);
    }
}

static void M_Add(
    const int16_t left, const int16_t top, const uint8_t angle,
    const int16_t speed, const PELLET_MOTION motion, const int16_t motion_speed)
{
    Pellets_AddSingle(
        g_Session.settings.difficulty, left, top, angle, speed, motion,
        motion_speed, 0, 0);
}

static void M_Half(void)
{
    if (g_B05.frame == 10) {
        g_B05.direction = MasterRng_Next(&g_Rng) % 2 == 1 ? 1 : -1;
    }
    if (g_B05.frame < 100) {
        M_Accelerate(g_B05.direction);
        return;
    }
    if (g_B05.frame == 100) {
        g_B05.pattern_state = Boss_Rank(10, 15, 20, 30);
        g_B05.angle = g_B05.direction == -1 ? 0 : 128;
    }
    if (g_B05.frame < 160) {
        M_Rotate(g_B05.direction, 1);
        if (g_B05.frame % (60 / g_B05.pattern_state) == 0) {
            M_Add(
                Math16_Wrap(g_B05.sphere.left + 44),
                Math16_Wrap(g_B05.sphere.top + 44), g_B05.angle, 50,
                PELLET_REGULAR, 0);
            g_B05.angle =
                (uint8_t)(g_B05.angle
                          - g_B05.direction * 128 / g_B05.pattern_state);
        }
    } else {
        g_B05.frame = 0;
    }
}

static void M_Slam(void)
{
    if (g_B05.frame < 100) {
        M_Accelerate(1);
        return;
    }
    const PLAYER *const player = &g_Player;
    if (g_B05.frame == 100) {
        g_B05.pattern_state = Boss_Rank(4, 4, 5, 6);
        const uint8_t angle = Math16_Angle(
            Math16_Wrap(368 - Math16_Wrap(g_B05.sphere.top + 32)),
            Math16_Wrap(player->left - Math16_Wrap(g_B05.sphere.left + 32)));
        g_B05.velocity_x = Math16_PolarX(0, g_B05.pattern_state, angle);
        g_B05.velocity_y = Math16_PolarY(0, g_B05.pattern_state, angle);
    }
    if (g_B05.velocity_x != 999) {
        M_Move(g_B05.velocity_x, g_B05.velocity_y);
        if (g_B05.sphere.top > 304) {
            g_B05.velocity_x = 999;
            g_B05.velocity_y = -4;
        }
    } else if (g_B05.velocity_y == -4) {
        M_Move(0, g_B05.velocity_y);
        if (g_B05.sphere.top < 96) {
            g_B05.velocity_y = 0;
        }
    } else {
        g_B05.frame = 0;
    }
    if (!player->invincible && g_B05.sphere.left <= player->left
        && Math16_Wrap(g_B05.sphere.left + 64) >= player->left
        && g_B05.sphere.top >= 272) {
        g_Player.is_hit = true;
    }
}

static void M_FireMan(const B05_FIRE fire)
{
    if (fire == B05_FIRE_NONE) {
        return;
    }
    g_B05.pattern_state = fire == B05_FIRE_DOWN ? Boss_Rank(48, 54, 60, 66)
                                                : Boss_Rank(48, 64, 80, 96);
    for (int32_t i = 0; i < 10; i++) {
        if (fire == B05_FIRE_DOWN) {
            const uint8_t angle = (uint8_t)(MasterRng_Next(&g_Rng) % 128);
            M_Add(
                Math16_Wrap(g_B05.sphere.left + 44),
                Math16_Wrap(g_B05.sphere.top + 44), angle, g_B05.pattern_state,
                PELLET_REGULAR, 0);
        } else {
            const int16_t dx = (int16_t)(MasterRng_Next(&g_Rng) % 96);
            const int16_t dy = (int16_t)(MasterRng_Next(&g_Rng) % 96);
            M_Add(
                Math16_Wrap(g_B05.sphere.left + dx),
                Math16_Wrap(g_B05.sphere.top + dy), 0, 0, PELLET_SLING_AIMED,
                g_B05.pattern_state);
        }
    }
}

static void M_Transform(const int16_t form, const B05_FIRE fire)
{
    if (g_B05.frame < 100) {
        M_Accelerate(1);
        return;
    }
    if (g_B05.frame == 100 || g_B05.frame == 250) {
        M_Put(&g_B05.flash, 0);
    } else if (
        g_B05.frame == 105 || g_B05.frame == 110 || g_B05.frame == 240
        || g_B05.frame == 245) {
        M_Put(&g_B05.flash, (int16_t)(1 + form));
    } else if (g_B05.frame == 115) {
        M_Put(&g_B05.person, (int16_t)(3 * form));
    } else if (g_B05.frame == 135) {
        g_B05.person.image = (int16_t)(1 + 3 * form);
        M_Put(&g_B05.person, g_B05.person.image);
        M_FireMan(fire);
    } else if (g_B05.frame == 160) {
        g_B05.person.image = (int16_t)(2 + 3 * form);
        M_Put(&g_B05.person, form == 0 ? 2 : 4);
        M_FireMan(fire);
    } else if (g_B05.frame == 185) {
        g_B05.person.image = (int16_t)(3 * form);
        M_Put(&g_B05.person, g_B05.person.image);
        M_FireMan(fire);
    } else if (g_B05.frame == 260) {
        M_Put(&g_B05.sphere, 0);
        g_B05.frame = 0;
    }
    if (g_B05.frame > 135 && g_B05.frame < 240 && g_B05.frame % 4 == 0) {
        M_Put(&g_B05.person, g_B05.person.image);
    }
}

static void M_Chase(void)
{
    M_Transform(0, B05_FIRE_NONE);
    if (g_B05.frame > 140 && g_B05.frame <= 220 && g_B05.frame % 8 == 0) {
        const int16_t speed = Boss_Rank(55, 58, 62, 65);
        M_Add(
            Math16_Wrap(g_B05.sphere.left + 44),
            Math16_Wrap(g_B05.sphere.top + 44), 0xf0, 16, PELLET_CHASE, speed);
        M_Add(
            Math16_Wrap(g_B05.sphere.left + 44),
            Math16_Wrap(g_B05.sphere.top + 44), 0x90, 16, PELLET_CHASE, speed);
    }
}

static void M_Cross(void)
{
    M_Transform(0, B05_FIRE_NONE);
    if (g_B05.frame > 140 && g_B05.frame % 8 == 0) {
        const int16_t speed = Boss_Rank(60, 70, 75, 80);
        M_Add(
            Math16_Wrap(g_B05.sphere.left + 12),
            Math16_Wrap(g_B05.sphere.top + 44), 0x30, speed, PELLET_REGULAR, 0);
        M_Add(
            Math16_Wrap(g_B05.sphere.left + 84),
            Math16_Wrap(g_B05.sphere.top + 44), 0x50, speed, PELLET_REGULAR, 0);
    }
}

static bool M_Entrance(const B05_CONTEXT *const c)
{
    for (;;) {
        if (g_B05.pending == B05_ENTRANCE_DELAY) {
            if (!Frame_DelayReady(1)) {
                return false;
            }
            if (g_B05.frame == 200) {
                g_B05.phase = B05_PHASE_SPHERE;
                g_B05.pattern = 0;
                g_B05.hit.invincible = false;
                g_B05.frame = 0;
                g_B05.hp_bar_filled = false;
                Palette_Set(c->hit->palette, &g_B05.boss_palette);
                *c->stage_palette = c->hit->palette->saved;
                g_B05.boss_palette = c->hit->palette->saved;
                g_B05.sphere.orb_inactive = false;
                g_B05.hit.frame = 0;
                g_B05.pattern_state = Boss_Rank(70, 50, 30, 10);
                g_B05.pending = B05_IDLE;
                return true;
            }
            g_B05.pending = B05_ENTRANCE;
        }
        BossEntity_LockedMovePut(&g_B05.sphere, 0, 0, 3, true);
        g_B05.frame++;
        if (g_B05.frame % g_B05.rotation_interval == 0) {
            M_Rotate(1, 1);
            g_B05.rotation_interval -= 2;
            if (g_B05.rotation_interval <= 0) {
                g_B05.rotation_interval = 1;
            }
        }
        if (g_B05.frame % 20 == 0) {
            const int32_t colors[] = { 5, 9, 15 };
            for (int32_t component = 0; component < 3; component++) {
                for (int32_t i = 0; i < 3; i++) {
                    int8_t *const value =
                        &c->hit->palette->saved.colors[colors[i]][component];
                    if (*value
                        < c->stage_palette->colors[colors[i]][component]) {
                        (*value)++;
                    }
                }
            }
            Palette_Set(c->hit->palette, &c->hit->palette->saved);
        }
        g_B05.pending = B05_ENTRANCE_DELAY;
    }
}

static bool M_Death(const B05_CONTEXT *const c, const TH01_INPUT *const raw)
{
    if (g_B05.pending == B05_DEFEAT) {
        const BOSS_DEFEAT_CONTEXT defeat = {
            .palette = c->hit->palette,
            .post_defeat = &g_B05.post_defeat,
            .miko = c->miko,
            .origin_top = &g_B05.sphere.top,
            .stage_cleared = c->route->stage_cleared,
        };
        if (!BossDefeat_Resume(&g_B05.defeat, &defeat)) {
            return false;
        }
        g_B05.pending = B05_ROUTE;
        if (!RouteSelect_Begin(&g_B05.route_selection, c->route, raw)) {
            return false;
        }
    } else if (!RouteSelect_Resume(&g_B05.route_selection, c->route, raw)) {
        return false;
    }
    g_B05.pending = B05_FINISHED;
    return true;
}

RESULT B05_ValidateAssets(const B05_ASSETS *const assets)
{
    const BOS *const banks[] = { &assets->sphere, &assets->flash,
                                 &assets->person };
    const int32_t minimum[] = { 8, 3, 5 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->width != 96 || banks[i]->height != 96
                || banks[i]->count < minimum[i] || banks[i]->count > 8,
            "Invalid SinGyoku sprite bank");
    }
    FAIL_IF(
        assets->background.width != VIDEO_WIDTH
            || assets->background.height != VIDEO_HEIGHT
            || assets->background.pixels == nullptr
            || !assets->background.has_palette,
        "Invalid SinGyoku background");
    return OK;
}

RESULT B05_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator, B05_ASSETS *const out)
{
    *out = (B05_ASSETS) {};
    B05_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS1.BOS", allocator, &loaded.sphere));
    MUST(Boss_LoadBos(vfs, "BOSS1_2.BOS", allocator, &loaded.flash));
    MUST(Boss_LoadBos(vfs, "BOSS1_3.BOS", allocator, &loaded.person));
    MUST(Boss_LoadBackground(vfs, "BOSS1.GRP", allocator, &loaded.background));
    MUST(B05_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B05_Init(void)
{
    memset(&g_B05, 0, sizeof(g_B05));
}

void B05_Setup(const B05_CONTEXT *const c)
{
    ASSERT(g_B05.pending == B05_IDLE || g_B05.pending == B05_FINISHED);
    ASSERT(Result_Ignore(B05_ValidateAssets(c->assets)));
    ASSERT(
        Result_Ignore(BossEntity_Bind(&g_B05.sphere, 0, &c->assets->sphere)));
    ASSERT(Result_Ignore(BossEntity_Bind(&g_B05.flash, 1, &c->assets->flash)));
    ASSERT(
        Result_Ignore(BossEntity_Bind(&g_B05.person, 2, &c->assets->person)));
    g_B05.post_defeat = c->assets->palette;
    *c->stage_palette = g_B05.post_defeat;
    Palette_Set(c->hit->palette, &g_B05.post_defeat);
    g_B05.boss_palette = c->hit->palette->saved;
    Palette_Set(c->hit->palette, &c->hit->palette->saved);
    BossEntity_SetPosition(
        &g_B05.sphere, 640, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&g_B05.sphere, (BOSS_BOUNDS) { 8, 88, 8, 88 });
    g_B05.sphere.orb_inactive = false;
    g_B05.sphere.image = 0;
    g_B05.hp = 8;
    g_HudHp.first_white = 6;
    g_HudHp.first_redwhite = 2;
    g_B05.phase = B05_PHASE_START;
    g_B05.pending = B05_IDLE;
    Particles_Init();
}

bool B05_UpdateAndRender(
    const B05_CONTEXT *const c, const TH01_INPUT *const raw)
{
    if (g_B05.pending == B05_FINISHED) {
        return true;
    }
    if (g_B05.pending == B05_ENTRANCE || g_B05.pending == B05_ENTRANCE_DELAY) {
        return M_Entrance(c);
    }
    if (g_B05.pending == B05_DEFEAT || g_B05.pending == B05_ROUTE) {
        return M_Death(c, raw);
    }
    if (g_B05.phase == B05_PHASE_START) {
        g_B05.sphere.left = 272;
        g_B05.sphere.top = 96;
        Palette_SetColor(c->hit->palette, 5, 0, 0, 0);
        Palette_SetColor(c->hit->palette, 9, 0, 0, 0);
        Palette_SetColor(c->hit->palette, 15, 0, 0, 0);
        g_B05.rotation_interval = 18;
        g_B05.frame = 0;
        g_B05.pending = B05_ENTRANCE;
        return M_Entrance(c);
    }
    if (g_B05.phase == B05_PHASE_SPHERE
        || g_B05.phase == B05_PHASE_TRANSFORMS) {
        if (g_B05.phase == B05_PHASE_SPHERE && !g_B05.hp_bar_filled) {
            g_B05.hp_bar_filled = HudHp_Increment(g_B05.hp, g_B05.hit.frame);
        }
        g_B05.frame = Math16_Wrap(g_B05.frame + 1);
        g_B05.hit.frame = Math16_Wrap(g_B05.hit.frame + 1);
        if (g_B05.phase == B05_PHASE_SPHERE) {
            if (g_B05.pattern == 0) {
                M_Half();
            } else if (g_B05.pattern == 1) {
                M_Slam();
            }
            if (g_B05.frame == 0) {
                g_B05.pattern =
                    g_B05.pattern == 1 ? 0 : Math16_Wrap(g_B05.pattern + 1);
            }
        } else {
            switch (g_B05.pattern) {
            case 0:
                M_Chase();
                break;
            case 1:
                M_Transform(1, B05_FIRE_DOWN);
                break;
            case 2:
                M_Cross();
                break;
            case 3:
                M_Transform(1, B05_FIRE_SLING);
                break;
            case 4:
                M_Slam();
                break;
            }
            if (g_B05.frame == 0) {
                g_B05.pattern = g_B05.pattern == 4
                    ? (int16_t)(MasterRng_Next(&g_Rng) % 4)
                    : 4;
            }
        }
        const uint8_t flash[] = { 13 };
        BossHit_UpdateAndRender(
            &g_B05.hit, c->hit, &g_B05.hp, &g_B05.boss_palette, flash, 1, 3000,
            BossEntity_HitTestOrb(&g_B05.sphere),
            (BOSS_SHOT_HITBOX) {
                Math16_Wrap(g_B05.sphere.left + 16),
                Math16_Wrap(g_B05.sphere.top + 32),
                96,
                48,
            });
        if (g_B05.phase == B05_PHASE_SPHERE && g_B05.hp <= 6
            && !g_B05.hit.invincible && g_B05.pattern != 1) {
            g_B05.phase = B05_PHASE_TRANSFORMS;
            g_B05.pattern = 0;
            g_B05.frame = 0;
            g_B05.hit.frame = 0;
        } else if (g_B05.phase == B05_PHASE_TRANSFORMS && g_B05.hp <= 0) {
            g_B05.phase = B05_PHASE_DEFEATED;
            Sound_Effect(5);
            g_B05.frame = 0;
        }
    } else if (g_B05.phase == B05_PHASE_DEFEATED) {
        BossEntity_Put(
            &g_B05.sphere, g_B05.sphere.left, g_B05.sphere.top,
            g_B05.sphere.image);
        Sound_Fade();
        Pellets_EraseAndResetMoving();
        BossDefeat_Begin(&g_B05.defeat, BOSS_DEFEAT_SINGYOKU);
        g_B05.pending = B05_DEFEAT;
        return M_Death(c, raw);
    }
    return true;
}
