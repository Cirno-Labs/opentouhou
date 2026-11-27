#include "th01/boss/private.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "pc98/zun/pi.h"
#include "th01/route_select.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "th01/rng.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"
#include "th01/sprite.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    BOS sphere;
    BOS flash;
    BOS person;
    PI_IMAGE background;
    PALETTE palette;
} B05_ASSETS;

typedef enum {
    B05_IDLE,
    B05_ENTRANCE,
    B05_ENTRANCE_DELAY,
    B05_DEFEAT,
    B05_ROUTE,
    B05_FINISHED,
} B05_PENDING;

typedef enum {
    B05_PHASE_START = 0,
    B05_PHASE_SPHERE = 1,
    B05_PHASE_TRANSFORMS = 2,
    B05_PHASE_DEFEATED = 8,
} B05_PHASE;

typedef struct {
    BOSS_ENTITY sphere;
    BOSS_ENTITY flash;
    BOSS_ENTITY person;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    BOSS_DEFEAT defeat;
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t velocity_x;
    int16_t velocity_y;
    int16_t rotation_interval;
    uint8_t angle;
    int8_t direction;
    bool hp_bar_filled;
    B05_PENDING pending;
} B05;

typedef enum {
    B05_FIRE_NONE,
    B05_FIRE_DOWN,
    B05_FIRE_SLING,
} B05_FIRE;

static B05 m_B05;

static void M_Put(const BOSS_ENTITY *const entity, const int16_t image)
{
    BossEntity_RestorePut(entity, m_B05.sphere.left, m_B05.sphere.top, image);
}

static void M_Rotate(const int16_t interval, const int16_t delta)
{
    if (m_B05.status.frame % interval != 0) {
        return;
    }
    const uint8_t bits = (uint8_t)(m_B05.sphere.image + delta);
    int16_t image = bits < 128 ? bits : (int16_t)bits - 256;
    if (image > 7) {
        image = 0;
    } else if (image < 0) {
        image = 7;
    }
    m_B05.sphere.image = image;
    M_Put(&m_B05.sphere, image);
}

static void M_Accelerate(const int16_t delta)
{
    if (m_B05.status.frame < 50) {
        m_B05.sphere.image = 0;
        if (m_B05.status.frame % 4 == 0) {
            M_Put(&m_B05.sphere, 0);
        }
        return;
    }
    if (m_B05.status.frame == 50) {
        Sound_Effect(SOUND_EFFECT_TELEGRAPH);
    }
    if (m_B05.status.frame < 100 && m_B05.status.frame % 4 == 0) {
        M_Put(&m_B05.sphere, m_B05.sphere.image);
        if (m_B05.status.frame == 60 || m_B05.status.frame == 68
            || m_B05.status.frame > 82) {
            M_Rotate(1, delta);
        }
    }
}

static void M_Move(const int16_t dx, const int16_t dy)
{
    BOSS_ENTITY *const e = &m_B05.sphere;
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
    if (m_B05.status.frame % 2 == 0) {
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
    if (m_B05.status.frame == 10) {
        m_B05.direction = MasterRng_Next(&g_Rng) % 2 == 1 ? 1 : -1;
    }
    if (m_B05.status.frame < 100) {
        M_Accelerate(m_B05.direction);
        return;
    }
    if (m_B05.status.frame == 100) {
        m_B05.pattern_state = Boss_Rank(10, 15, 20, 30);
        m_B05.angle = m_B05.direction == -1 ? 0 : 128;
    }
    if (m_B05.status.frame < 160) {
        M_Rotate(m_B05.direction, 1);
        if (m_B05.status.frame % (60 / m_B05.pattern_state) == 0) {
            M_Add(
                Math16_Wrap(m_B05.sphere.left + 44),
                Math16_Wrap(m_B05.sphere.top + 44), m_B05.angle, 50,
                PELLET_REGULAR, 0);
            m_B05.angle =
                (uint8_t)(m_B05.angle
                          - m_B05.direction * 128 / m_B05.pattern_state);
        }
    } else {
        m_B05.status.frame = 0;
    }
}

static void M_Slam(void)
{
    if (m_B05.status.frame < 100) {
        M_Accelerate(1);
        return;
    }
    const PLAYER *const player = g_Player;
    if (m_B05.status.frame == 100) {
        m_B05.pattern_state = Boss_Rank(4, 4, 5, 6);
        const uint8_t angle = Math16_Angle(
            Math16_Wrap(368 - Math16_Wrap(m_B05.sphere.top + 32)),
            Math16_Wrap(player->left - Math16_Wrap(m_B05.sphere.left + 32)));
        m_B05.velocity_x = Math16_PolarX(0, m_B05.pattern_state, angle);
        m_B05.velocity_y = Math16_PolarY(0, m_B05.pattern_state, angle);
    }
    if (m_B05.velocity_x != 999) {
        M_Move(m_B05.velocity_x, m_B05.velocity_y);
        if (m_B05.sphere.top > 304) {
            m_B05.velocity_x = 999;
            m_B05.velocity_y = -4;
        }
    } else if (m_B05.velocity_y == -4) {
        M_Move(0, m_B05.velocity_y);
        if (m_B05.sphere.top < 96) {
            m_B05.velocity_y = 0;
        }
    } else {
        m_B05.status.frame = 0;
    }
    if (!player->invincible && m_B05.sphere.left <= player->left
        && Math16_Wrap(m_B05.sphere.left + 64) >= player->left
        && m_B05.sphere.top >= 272) {
        Player_Hit();
    }
}

static void M_FireMan(const B05_FIRE fire)
{
    if (fire == B05_FIRE_NONE) {
        return;
    }
    m_B05.pattern_state = fire == B05_FIRE_DOWN ? Boss_Rank(48, 54, 60, 66)
                                                : Boss_Rank(48, 64, 80, 96);
    for (int32_t i = 0; i < 10; i++) {
        if (fire == B05_FIRE_DOWN) {
            const uint8_t angle = (uint8_t)(MasterRng_Next(&g_Rng) % 128);
            M_Add(
                Math16_Wrap(m_B05.sphere.left + 44),
                Math16_Wrap(m_B05.sphere.top + 44), angle, m_B05.pattern_state,
                PELLET_REGULAR, 0);
        } else {
            const int16_t dx = (int16_t)(MasterRng_Next(&g_Rng) % 96);
            const int16_t dy = (int16_t)(MasterRng_Next(&g_Rng) % 96);
            M_Add(
                Math16_Wrap(m_B05.sphere.left + dx),
                Math16_Wrap(m_B05.sphere.top + dy), 0, 0, PELLET_SLING_AIMED,
                m_B05.pattern_state);
        }
    }
}

static void M_Transform(const int16_t form, const B05_FIRE fire)
{
    if (m_B05.status.frame < 100) {
        M_Accelerate(1);
        return;
    }
    if (m_B05.status.frame == 100 || m_B05.status.frame == 250) {
        M_Put(&m_B05.flash, 0);
    } else if (
        m_B05.status.frame == 105 || m_B05.status.frame == 110
        || m_B05.status.frame == 240 || m_B05.status.frame == 245) {
        M_Put(&m_B05.flash, (int16_t)(1 + form));
    } else if (m_B05.status.frame == 115) {
        M_Put(&m_B05.person, (int16_t)(3 * form));
    } else if (m_B05.status.frame == 135) {
        m_B05.person.image = (int16_t)(1 + 3 * form);
        M_Put(&m_B05.person, m_B05.person.image);
        M_FireMan(fire);
    } else if (m_B05.status.frame == 160) {
        m_B05.person.image = (int16_t)(2 + 3 * form);
        M_Put(&m_B05.person, form == 0 ? 2 : 4);
        M_FireMan(fire);
    } else if (m_B05.status.frame == 185) {
        m_B05.person.image = (int16_t)(3 * form);
        M_Put(&m_B05.person, m_B05.person.image);
        M_FireMan(fire);
    } else if (m_B05.status.frame == 260) {
        M_Put(&m_B05.sphere, 0);
        m_B05.status.frame = 0;
    }
    if (m_B05.status.frame > 135 && m_B05.status.frame < 240
        && m_B05.status.frame % 4 == 0) {
        M_Put(&m_B05.person, m_B05.person.image);
    }
}

static void M_Chase(void)
{
    M_Transform(0, B05_FIRE_NONE);
    if (m_B05.status.frame > 140 && m_B05.status.frame <= 220
        && m_B05.status.frame % 8 == 0) {
        const int16_t speed = Boss_Rank(55, 58, 62, 65);
        M_Add(
            Math16_Wrap(m_B05.sphere.left + 44),
            Math16_Wrap(m_B05.sphere.top + 44), 0xf0, 16, PELLET_CHASE, speed);
        M_Add(
            Math16_Wrap(m_B05.sphere.left + 44),
            Math16_Wrap(m_B05.sphere.top + 44), 0x90, 16, PELLET_CHASE, speed);
    }
}

static void M_Cross(void)
{
    M_Transform(0, B05_FIRE_NONE);
    if (m_B05.status.frame > 140 && m_B05.status.frame % 8 == 0) {
        const int16_t speed = Boss_Rank(60, 70, 75, 80);
        M_Add(
            Math16_Wrap(m_B05.sphere.left + 12),
            Math16_Wrap(m_B05.sphere.top + 44), 0x30, speed, PELLET_REGULAR, 0);
        M_Add(
            Math16_Wrap(m_B05.sphere.left + 84),
            Math16_Wrap(m_B05.sphere.top + 44), 0x50, speed, PELLET_REGULAR, 0);
    }
}

static bool M_Entrance(void)
{
    for (;;) {
        if (m_B05.pending == B05_ENTRANCE_DELAY) {
            if (!VSyncIrq_DelayReady(1)) {
                return false;
            }
            if (m_B05.status.frame == 200) {
                m_B05.status.phase = B05_PHASE_SPHERE;
                m_B05.pattern = 0;
                m_B05.hit.invincible = false;
                m_B05.status.frame = 0;
                m_B05.hp_bar_filled = false;
                Palette_Set(&g_Palette, &m_B05.boss_palette);
                Palette_SetStage(&g_Palette.saved);
                m_B05.boss_palette = g_Palette.saved;
                m_B05.sphere.orb_inactive = false;
                m_B05.hit.frame = 0;
                m_B05.pattern_state = Boss_Rank(70, 50, 30, 10);
                m_B05.pending = B05_IDLE;
                return true;
            }
            m_B05.pending = B05_ENTRANCE;
        }
        BossEntity_LockedMovePut(&m_B05.sphere, 0, 0, 3, true);
        m_B05.status.frame++;
        if (m_B05.status.frame % m_B05.rotation_interval == 0) {
            M_Rotate(1, 1);
            m_B05.rotation_interval -= 2;
            if (m_B05.rotation_interval <= 0) {
                m_B05.rotation_interval = 1;
            }
        }
        if (m_B05.status.frame % 20 == 0) {
            const int32_t colors[] = { 5, 9, 15 };
            for (int32_t component = 0; component < 3; component++) {
                for (int32_t i = 0; i < 3; i++) {
                    int8_t *const value =
                        &g_Palette.saved.colors[colors[i]][component];
                    if (*value < g_StagePalette.colors[colors[i]][component]) {
                        (*value)++;
                    }
                }
            }
            Palette_Set(&g_Palette, &g_Palette.saved);
        }
        m_B05.pending = B05_ENTRANCE_DELAY;
    }
}

static bool M_Death(
    const PTN *const miko, const ROUTE_SELECT_ASSETS *const route,
    const TH01_INPUT *const raw)
{
    if (m_B05.pending == B05_DEFEAT) {
        if (!BossDefeat_Resume(
                &m_B05.defeat, &m_B05.post_defeat, miko, &m_B05.sphere.top)) {
            return false;
        }
        m_B05.pending = B05_ROUTE;
        if (!RouteSelect_Begin(route, raw)) {
            return false;
        }
    } else if (!RouteSelect_Resume(route, raw)) {
        return false;
    }
    m_B05.pending = B05_FINISHED;
    return true;
}

static RESULT M_ValidateAssets(const B05_ASSETS *const assets)
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

static RESULT M_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator, void *const data)
{
    B05_ASSETS *const out = data;
    *out = (B05_ASSETS) {};
    B05_ASSETS loaded = {};
    MUST(Boss_LoadBos(vfs, "BOSS1.BOS", allocator, &loaded.sphere));
    MUST(Boss_LoadBos(vfs, "BOSS1_2.BOS", allocator, &loaded.flash));
    MUST(Boss_LoadBos(vfs, "BOSS1_3.BOS", allocator, &loaded.person));
    MUST(Boss_LoadBackground(vfs, "BOSS1.GRP", allocator, &loaded.background));
    MUST(M_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

static void M_Init(void)
{
    memset(&m_B05, 0, sizeof(m_B05));
    RouteSelect_Init();
}

static void M_Setup(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const B05_ASSETS *const assets = data;
    Boss_Select(&m_B05.status);
    ASSERT(m_B05.pending == B05_IDLE || m_B05.pending == B05_FINISHED);
    ASSERT(Result_Ignore(M_ValidateAssets(assets)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B05.sphere, 0, &assets->sphere)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B05.flash, 1, &assets->flash)));
    ASSERT(Result_Ignore(BossEntity_Bind(&m_B05.person, 2, &assets->person)));
    m_B05.post_defeat = assets->palette;
    Palette_SetStage(&m_B05.post_defeat);
    Palette_Set(&g_Palette, &m_B05.post_defeat);
    m_B05.boss_palette = g_Palette.saved;
    Palette_Set(&g_Palette, &g_Palette.saved);
    BossEntity_SetPosition(
        &m_B05.sphere, 640, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&m_B05.sphere, (BOSS_BOUNDS) { 8, 88, 8, 88 });
    m_B05.sphere.orb_inactive = false;
    m_B05.sphere.image = 0;
    m_B05.status.hp = 8;
    HudHp_Begin(2, 6);
    m_B05.status.phase = B05_PHASE_START;
    m_B05.pending = B05_IDLE;
    Particles_Init();
}

static bool M_Update(const void *const data, const BOSS_CONTEXT *const ctx)
{
    const PTN *const miko = ctx->miko;
    const ROUTE_SELECT_ASSETS *const route = ctx->route;
    const TH01_INPUT *const raw = ctx->raw;
    if (m_B05.pending == B05_FINISHED) {
        return true;
    }
    if (m_B05.pending == B05_ENTRANCE || m_B05.pending == B05_ENTRANCE_DELAY) {
        return M_Entrance();
    }
    if (m_B05.pending == B05_DEFEAT || m_B05.pending == B05_ROUTE) {
        return M_Death(miko, route, raw);
    }
    if (m_B05.status.phase == B05_PHASE_START) {
        m_B05.sphere.left = 272;
        m_B05.sphere.top = 96;
        Palette_SetColor(&g_Palette, 5, 0, 0, 0);
        Palette_SetColor(&g_Palette, 9, 0, 0, 0);
        Palette_SetColor(&g_Palette, 15, 0, 0, 0);
        m_B05.rotation_interval = 18;
        m_B05.status.frame = 0;
        m_B05.pending = B05_ENTRANCE;
        return M_Entrance();
    }
    if (m_B05.status.phase == B05_PHASE_SPHERE
        || m_B05.status.phase == B05_PHASE_TRANSFORMS) {
        if (m_B05.status.phase == B05_PHASE_SPHERE && !m_B05.hp_bar_filled) {
            m_B05.hp_bar_filled =
                HudHp_Increment(m_B05.status.hp, m_B05.hit.frame);
        }
        m_B05.status.frame = Math16_Wrap(m_B05.status.frame + 1);
        m_B05.hit.frame = Math16_Wrap(m_B05.hit.frame + 1);
        if (m_B05.status.phase == B05_PHASE_SPHERE) {
            if (m_B05.pattern == 0) {
                M_Half();
            } else if (m_B05.pattern == 1) {
                M_Slam();
            }
            if (m_B05.status.frame == 0) {
                m_B05.pattern =
                    m_B05.pattern == 1 ? 0 : Math16_Wrap(m_B05.pattern + 1);
            }
        } else {
            switch (m_B05.pattern) {
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
            if (m_B05.status.frame == 0) {
                m_B05.pattern = m_B05.pattern == 4
                    ? (int16_t)(MasterRng_Next(&g_Rng) % 4)
                    : 4;
            }
        }
        const uint8_t flash[] = { 13 };
        BossHit_UpdateAndRender(
            &m_B05.hit, &m_B05.status.hp, &m_B05.boss_palette, flash, 1, 3000,
            BossEntity_HitTestOrb(&m_B05.sphere),
            (BOSS_SHOT_HITBOX) {
                Math16_Wrap(m_B05.sphere.left + 16),
                Math16_Wrap(m_B05.sphere.top + 32),
                96,
                48,
            });
        if (m_B05.status.phase == B05_PHASE_SPHERE && m_B05.status.hp <= 6
            && !m_B05.hit.invincible && m_B05.pattern != 1) {
            m_B05.status.phase = B05_PHASE_TRANSFORMS;
            m_B05.pattern = 0;
            m_B05.status.frame = 0;
            m_B05.hit.frame = 0;
        } else if (
            m_B05.status.phase == B05_PHASE_TRANSFORMS
            && m_B05.status.hp <= 0) {
            m_B05.status.phase = B05_PHASE_DEFEATED;
            Sound_Effect(SOUND_EFFECT_MISS);
            m_B05.status.frame = 0;
        }
    } else if (m_B05.status.phase == B05_PHASE_DEFEATED) {
        BossEntity_Put(
            &m_B05.sphere, m_B05.sphere.left, m_B05.sphere.top,
            m_B05.sphere.image);
        Sound_Fade();
        Pellets_EraseAndResetMoving();
        BossDefeat_Begin(&m_B05.defeat, BOSS_DEFEAT_SINGYOKU);
        m_B05.pending = B05_DEFEAT;
        return M_Death(miko, route, raw);
    }
    return true;
}

static const PI_IMAGE *M_StageBackground(const void *const data)
{
    const B05_ASSETS *const assets = data;
    return &assets->background;
}

const BOSS_STRATEGY g_BossSingyoku = {
    .assets_size = sizeof(B05_ASSETS),
    .load_assets = M_LoadAssets,
    .init = M_Init,
    .setup = M_Setup,
    .update = M_Update,
    .background = M_StageBackground,
    .update_on_life_begin = true,
    .song = 7,
};
