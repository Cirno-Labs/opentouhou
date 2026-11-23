#include "th01/boss_b05.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"
#include "th01/sprite.h"

#include "core/base/assert.h"

typedef enum {
    B05_FIRE_NONE,
    B05_FIRE_DOWN,
    B05_FIRE_SLING,
} B05_FIRE;

static int16_t M_Rank(
    const B05_CONTEXT *const c, const int16_t easy, const int16_t normal,
    const int16_t hard, const int16_t lunatic)
{
    switch (c->hit->display->rank) {
    case SETTINGS_DIFFICULTY_EASY:
        return easy;
    case SETTINGS_DIFFICULTY_HARD:
        return hard;
    case SETTINGS_DIFFICULTY_LUNATIC:
        return lunatic;
    default:
        return normal;
    }
}

static void M_Put(
    const B05 *const b, const B05_CONTEXT *const c,
    const BOSS_ENTITY *const entity, const int16_t image)
{
    BossEntity_RestorePut(
        entity, c->sprites, b->sphere.left, b->sphere.top, image);
}

static void M_Rotate(
    B05 *const b, const B05_CONTEXT *const c, const int16_t interval,
    const int16_t delta)
{
    if (b->frame % interval != 0) {
        return;
    }
    const uint8_t bits = (uint8_t)(b->sphere.image + delta);
    int16_t image = bits < 128 ? bits : (int16_t)bits - 256;
    if (image > 7) {
        image = 0;
    } else if (image < 0) {
        image = 7;
    }
    b->sphere.image = image;
    M_Put(b, c, &b->sphere, image);
}

static void M_Accelerate(
    B05 *const b, const B05_CONTEXT *const c, const int16_t delta)
{
    if (b->frame < 50) {
        b->sphere.image = 0;
        if (b->frame % 4 == 0) {
            M_Put(b, c, &b->sphere, 0);
        }
        return;
    }
    if (b->frame == 50) {
        c->hit->effect(c->hit->user, 8);
    }
    if (b->frame < 100 && b->frame % 4 == 0) {
        M_Put(b, c, &b->sphere, b->sphere.image);
        if (b->frame == 60 || b->frame == 68 || b->frame > 82) {
            M_Rotate(b, c, 1, delta);
        }
    }
}

static void M_Move(
    B05 *const b, const B05_CONTEXT *const c, const int16_t dx,
    const int16_t dy)
{
    BOSS_ENTITY *const e = &b->sphere;
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
    if (b->frame % 2 == 0) {
        M_Rotate(b, c, 1, 1);
    }
}

static void M_Add(
    const B05_CONTEXT *const c, const int16_t left, const int16_t top,
    const uint8_t angle, const int16_t speed, const PELLET_MOTION motion,
    const int16_t motion_speed)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, (int32_t)c->hit->display->rank, left, top,
        angle, speed, motion, motion_speed, 0, 0);
}

static void M_Half(B05 *const b, const B05_CONTEXT *const c)
{
    if (b->frame == 10) {
        b->direction = MasterRng_Next(c->rng) % 2 == 1 ? 1 : -1;
    }
    if (b->frame < 100) {
        M_Accelerate(b, c, b->direction);
        return;
    }
    if (b->frame == 100) {
        b->pattern_state = M_Rank(c, 10, 15, 20, 30);
        b->angle = b->direction == -1 ? 0 : 128;
    }
    if (b->frame < 160) {
        M_Rotate(b, c, b->direction, 1);
        if (b->frame % (60 / b->pattern_state) == 0) {
            M_Add(
                c, Math16_Wrap(b->sphere.left + 44),
                Math16_Wrap(b->sphere.top + 44), b->angle, 50, PELLET_REGULAR,
                0);
            b->angle =
                (uint8_t)(b->angle - b->direction * 128 / b->pattern_state);
        }
    } else {
        b->frame = 0;
    }
}

static void M_Slam(B05 *const b, const B05_CONTEXT *const c)
{
    if (b->frame < 100) {
        M_Accelerate(b, c, 1);
        return;
    }
    const PLAYER *const player = c->hit->display->player;
    if (b->frame == 100) {
        b->pattern_state = M_Rank(c, 4, 4, 5, 6);
        const uint8_t angle = Math16_Angle(
            Math16_Wrap(368 - Math16_Wrap(b->sphere.top + 32)),
            Math16_Wrap(player->left - Math16_Wrap(b->sphere.left + 32)));
        b->velocity_x = Math16_PolarX(0, b->pattern_state, angle);
        b->velocity_y = Math16_PolarY(0, b->pattern_state, angle);
    }
    if (b->velocity_x != 999) {
        M_Move(b, c, b->velocity_x, b->velocity_y);
        if (b->sphere.top > 304) {
            b->velocity_x = 999;
            b->velocity_y = -4;
        }
    } else if (b->velocity_y == -4) {
        M_Move(b, c, 0, b->velocity_y);
        if (b->sphere.top < 96) {
            b->velocity_y = 0;
        }
    } else {
        b->frame = 0;
    }
    if (!player->invincible && b->sphere.left <= player->left
        && Math16_Wrap(b->sphere.left + 64) >= player->left
        && b->sphere.top >= 272) {
        c->hit->display->player->is_hit = true;
    }
}

static void M_FireMan(
    B05 *const b, const B05_CONTEXT *const c, const B05_FIRE fire)
{
    if (fire == B05_FIRE_NONE) {
        return;
    }
    b->pattern_state = fire == B05_FIRE_DOWN ? M_Rank(c, 48, 54, 60, 66)
                                             : M_Rank(c, 48, 64, 80, 96);
    for (int32_t i = 0; i < 10; i++) {
        if (fire == B05_FIRE_DOWN) {
            const uint8_t angle = (uint8_t)(MasterRng_Next(c->rng) % 128);
            M_Add(
                c, Math16_Wrap(b->sphere.left + 44),
                Math16_Wrap(b->sphere.top + 44), angle, b->pattern_state,
                PELLET_REGULAR, 0);
        } else {
            const int16_t dx = (int16_t)(MasterRng_Next(c->rng) % 96);
            const int16_t dy = (int16_t)(MasterRng_Next(c->rng) % 96);
            M_Add(
                c, Math16_Wrap(b->sphere.left + dx),
                Math16_Wrap(b->sphere.top + dy), 0, 0, PELLET_SLING_AIMED,
                b->pattern_state);
        }
    }
}

static void M_Transform(
    B05 *const b, const B05_CONTEXT *const c, const int16_t form,
    const B05_FIRE fire)
{
    if (b->frame < 100) {
        M_Accelerate(b, c, 1);
        return;
    }
    if (b->frame == 100 || b->frame == 250) {
        M_Put(b, c, &b->flash, 0);
    } else if (
        b->frame == 105 || b->frame == 110 || b->frame == 240
        || b->frame == 245) {
        M_Put(b, c, &b->flash, (int16_t)(1 + form));
    } else if (b->frame == 115) {
        M_Put(b, c, &b->person, (int16_t)(3 * form));
    } else if (b->frame == 135) {
        b->person.image = (int16_t)(1 + 3 * form);
        M_Put(b, c, &b->person, b->person.image);
        M_FireMan(b, c, fire);
    } else if (b->frame == 160) {
        b->person.image = (int16_t)(2 + 3 * form);
        M_Put(b, c, &b->person, form == 0 ? 2 : 4);
        M_FireMan(b, c, fire);
    } else if (b->frame == 185) {
        b->person.image = (int16_t)(3 * form);
        M_Put(b, c, &b->person, b->person.image);
        M_FireMan(b, c, fire);
    } else if (b->frame == 260) {
        M_Put(b, c, &b->sphere, 0);
        b->frame = 0;
    }
    if (b->frame > 135 && b->frame < 240 && b->frame % 4 == 0) {
        M_Put(b, c, &b->person, b->person.image);
    }
}

static void M_Chase(B05 *const b, const B05_CONTEXT *const c)
{
    M_Transform(b, c, 0, B05_FIRE_NONE);
    if (b->frame > 140 && b->frame <= 220 && b->frame % 8 == 0) {
        const int16_t speed = M_Rank(c, 55, 58, 62, 65);
        M_Add(
            c, Math16_Wrap(b->sphere.left + 44),
            Math16_Wrap(b->sphere.top + 44), 0xf0, 16, PELLET_CHASE, speed);
        M_Add(
            c, Math16_Wrap(b->sphere.left + 44),
            Math16_Wrap(b->sphere.top + 44), 0x90, 16, PELLET_CHASE, speed);
    }
}

static void M_Cross(B05 *const b, const B05_CONTEXT *const c)
{
    M_Transform(b, c, 0, B05_FIRE_NONE);
    if (b->frame > 140 && b->frame % 8 == 0) {
        const int16_t speed = M_Rank(c, 60, 70, 75, 80);
        M_Add(
            c, Math16_Wrap(b->sphere.left + 12),
            Math16_Wrap(b->sphere.top + 44), 0x30, speed, PELLET_REGULAR, 0);
        M_Add(
            c, Math16_Wrap(b->sphere.left + 84),
            Math16_Wrap(b->sphere.top + 44), 0x50, speed, PELLET_REGULAR, 0);
    }
}

static bool M_Entrance(B05 *const b, const B05_CONTEXT *const c)
{
    for (;;) {
        if (b->pending == B05_ENTRANCE_DELAY) {
            if (!Frame_DelayReady(1)) {
                return false;
            }
            if (b->frame == 200) {
                b->phase = 1;
                b->pattern = 0;
                b->hit.invincible = false;
                b->frame = 0;
                b->hp_bar_filled = false;
                Palette_Set(c->hit->palette, &b->boss_palette);
                *c->stage_palette = c->hit->palette->saved;
                b->boss_palette = c->hit->palette->saved;
                b->sphere.orb_inactive = false;
                b->hit.frame = 0;
                b->pattern_state = M_Rank(c, 70, 50, 30, 10);
                b->pending = B05_IDLE;
                return true;
            }
            b->pending = B05_ENTRANCE;
        }
        BossEntity_LockedMovePut(&b->sphere, c->sprites, 0, 0, 3, true);
        b->frame++;
        if (b->frame % b->rotation_interval == 0) {
            M_Rotate(b, c, 1, 1);
            b->rotation_interval -= 2;
            if (b->rotation_interval <= 0) {
                b->rotation_interval = 1;
            }
        }
        if (b->frame % 20 == 0) {
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
        b->pending = B05_ENTRANCE_DELAY;
    }
}

static bool M_Death(
    B05 *const b, const B05_CONTEXT *const c, const TH01_INPUT *const raw)
{
    if (b->pending == B05_DEFEAT) {
        const BOSS_DEFEAT_CONTEXT defeat = {
            .palette = c->hit->palette,
            .post_defeat = &b->post_defeat,
            .miko = c->miko,
            .player = c->hit->display->player,
            .origin_top = &b->sphere.top,
            .stage_cleared = c->route->stage_cleared,
        };
        if (!BossDefeat_Resume(&b->defeat, &defeat)) {
            return false;
        }
        b->pending = B05_ROUTE;
        if (!RouteSelect_Begin(&b->route_selection, c->route, raw)) {
            return false;
        }
    } else if (!RouteSelect_Resume(&b->route_selection, c->route, raw)) {
        return false;
    }
    b->pending = B05_FINISHED;
    return true;
}

static RESULT M_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const allocator,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    RESULT result = Bos_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const allocator, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS1.GRP", VFS_IN_MEMORY, &reader));
    RESULT result = Pi_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
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
    MUST(M_LoadBos(vfs, "BOSS1.BOS", allocator, &loaded.sphere));
    MUST(M_LoadBos(vfs, "BOSS1_2.BOS", allocator, &loaded.flash));
    MUST(M_LoadBos(vfs, "BOSS1_3.BOS", allocator, &loaded.person));
    MUST(M_LoadBackground(vfs, allocator, &loaded.background));
    MUST(B05_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B05_Setup(B05 *const boss, const B05_CONTEXT *const c)
{
    ASSERT(boss->pending == B05_IDLE || boss->pending == B05_FINISHED);
    ASSERT(Result_Ignore(B05_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&boss->sphere, c->sprites, 0, &c->assets->sphere)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&boss->flash, c->sprites, 1, &c->assets->flash)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&boss->person, c->sprites, 2, &c->assets->person)));
    boss->post_defeat = c->assets->palette;
    *c->stage_palette = boss->post_defeat;
    Palette_Set(c->hit->palette, &boss->post_defeat);
    boss->boss_palette = c->hit->palette->saved;
    Palette_Set(c->hit->palette, &c->hit->palette->saved);
    BossEntity_SetPosition(
        &boss->sphere, 640, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&boss->sphere, (BOSS_BOUNDS) { 8, 88, 8, 88 });
    boss->sphere.orb_inactive = false;
    boss->sphere.image = 0;
    boss->hp = 8;
    c->hit->health->first_white = 6;
    c->hit->health->first_redwhite = 2;
    boss->phase = 0;
    boss->pending = B05_IDLE;
    Particles_Init(c->particles);
}

RESULT B05_ValidateContext(const B05_CONTEXT *const c)
{
    FAIL_IF(
        c == nullptr || c->assets == nullptr || c->sprites == nullptr
            || c->hit == nullptr || c->hit->display == nullptr
            || c->hit->palette == nullptr || c->hit->health == nullptr
            || c->hit->hud == nullptr || c->hit->shots_output.action == nullptr
            || c->hit->display->remaining_lives == nullptr
            || c->hit->display->high_score == nullptr
            || c->hit->display->font == nullptr
            || c->hit->display->sprites == nullptr
            || c->hit->display->player == nullptr
            || c->hit->display->gameplay == nullptr || c->hit->shots == nullptr
            || c->hit->orb == nullptr || c->hit->test_damage == nullptr
            || c->hit->effect == nullptr || c->stage_palette == nullptr
            || c->particles == nullptr || c->pellets == nullptr
            || c->pellet_shared == nullptr || c->rng == nullptr
            || c->miko == nullptr || c->route == nullptr,
        "Missing SinGyoku context");
    MUST(B05_ValidateAssets(c->assets));
    FAIL_IF(
        c->hit->health->backgrounds == nullptr
            || c->hit->hud->numbers == nullptr,
        "Missing SinGyoku HUD storage");
    MUST(Shots_ValidateSprites(c->hit->display->sprites));
    for (uint32_t digit = 0xff10; digit <= 0xff19; digit++) {
        FAIL_IF(
            GameText_Find(c->hit->display->font, digit) == nullptr,
            "Missing SinGyoku score glyph");
    }
    MUST(BossDefeat_ValidateSprites(c->miko));
    FAIL_IF(
        c->route->assets == nullptr || c->route->scene == nullptr
            || c->route->route == nullptr || c->route->stage_cleared == nullptr
            || c->route->effect == nullptr || c->fade_music == nullptr,
        "Missing SinGyoku route or music consumer");
    MUST(RouteSelect_ValidateAssets(c->route->assets));
    FAIL_IF(
        c->route->palette != c->hit->palette
            || c->route->player != c->hit->display->player,
        "Inconsistent SinGyoku context");
    return OK;
}

bool B05_UpdateAndRender(
    B05 *const b, const B05_CONTEXT *const c, const TH01_INPUT *const raw)
{
    if (b->pending == B05_FINISHED) {
        return true;
    }
    if (b->pending == B05_ENTRANCE || b->pending == B05_ENTRANCE_DELAY) {
        return M_Entrance(b, c);
    }
    if (b->pending == B05_DEFEAT || b->pending == B05_ROUTE) {
        return M_Death(b, c, raw);
    }
    if (b->phase == 0) {
        b->sphere.left = 272;
        b->sphere.top = 96;
        Palette_SetColor(c->hit->palette, 5, 0, 0, 0);
        Palette_SetColor(c->hit->palette, 9, 0, 0, 0);
        Palette_SetColor(c->hit->palette, 15, 0, 0, 0);
        b->rotation_interval = 18;
        b->frame = 0;
        b->pending = B05_ENTRANCE;
        return M_Entrance(b, c);
    }
    if (b->phase == 1 || b->phase == 2) {
        if (b->phase == 1 && !b->hp_bar_filled) {
            b->hp_bar_filled =
                HudHp_Increment(c->hit->health, b->hp, b->hit.frame);
        }
        b->frame = Math16_Wrap(b->frame + 1);
        b->hit.frame = Math16_Wrap(b->hit.frame + 1);
        if (b->phase == 1) {
            if (b->pattern == 0) {
                M_Half(b, c);
            } else if (b->pattern == 1) {
                M_Slam(b, c);
            }
            if (b->frame == 0) {
                b->pattern = b->pattern == 1 ? 0 : Math16_Wrap(b->pattern + 1);
            }
        } else {
            switch (b->pattern) {
            case 0:
                M_Chase(b, c);
                break;
            case 1:
                M_Transform(b, c, 1, B05_FIRE_DOWN);
                break;
            case 2:
                M_Cross(b, c);
                break;
            case 3:
                M_Transform(b, c, 1, B05_FIRE_SLING);
                break;
            case 4:
                M_Slam(b, c);
                break;
            }
            if (b->frame == 0) {
                b->pattern =
                    b->pattern == 4 ? (int16_t)(MasterRng_Next(c->rng) % 4) : 4;
            }
        }
        BOSS_HIT_CONTEXT hit = *c->hit;
        hit.hp = &b->hp;
        hit.boss_palette = &b->boss_palette;
        const uint8_t flash[] = { 13 };
        BossHit_UpdateAndRender(
            &b->hit, &hit, flash, 1, 3000,
            BossEntity_HitTestOrb(&b->sphere, hit.orb),
            (BOSS_SHOT_HITBOX) {
                Math16_Wrap(b->sphere.left + 16),
                Math16_Wrap(b->sphere.top + 32),
                96,
                48,
            });
        if (b->phase == 1 && b->hp <= 6 && !b->hit.invincible
            && b->pattern != 1) {
            b->phase = 2;
            b->pattern = 0;
            b->frame = 0;
            b->hit.frame = 0;
        } else if (b->phase == 2 && b->hp <= 0) {
            b->phase = 8;
            c->hit->effect(c->hit->user, 5);
            b->frame = 0;
        }
    } else if (b->phase == 8) {
        BossEntity_Put(
            &b->sphere, c->sprites, b->sphere.left, b->sphere.top,
            b->sphere.image);
        c->fade_music(c->hit->user);
        Pellets_EraseAndResetMoving(c->pellets);
        BossDefeat_Begin(&b->defeat, BOSS_DEFEAT_SINGYOKU);
        b->pending = B05_DEFEAT;
        return M_Death(b, c, raw);
    }
    return true;
}
