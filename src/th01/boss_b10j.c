#include "th01/boss_b10j.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/assert.h"
#include "th01/shape.h"
#include "th01/sprite.h"

static RESULT M_LoadBos(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    BOS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Bos_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadBackground(
    VFS *const vfs, const ALLOCATOR *const a, PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3.GRP", VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_LoadMissiles(
    VFS *const vfs, const ALLOCATOR *const a, PTN *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS3_M.PTN", VFS_IN_MEMORY, &reader));
    const RESULT result = Ptn_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}

static RESULT M_ReadPillars(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, const B10J_PILLARS **const out)
{
    READER image;
    MUST(ReiidenImage_Open(reader, scratch, scratch_size, &image));
    B10J_PILLARS masks;
    Reader_Seek(&image, 0x3937d);
    Reader_ReadBytes(&image, masks.masks, sizeof(masks.masks));
    MUST(Reader_Result(&image));
    B10J_PILLARS *const allocated = Allocator_Alloc(a, sizeof(masks));
    *allocated = masks;
    *out = allocated;
    return OK;
}

static RESULT M_LoadPillars(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, const B10J_PILLARS **const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    const RESULT result = M_ReadPillars(&reader, scratch, scratch_size, a, out);
    Reader_Close(&reader);
    return result;
}

RESULT B10J_ValidateAssets(const B10J_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->still.pixels == nullptr
            || a->still.coverage == nullptr || a->still.width != 128
            || a->still.height != 160 || a->still.count != 1,
        "Invalid Mima still bank");
    FAIL_IF(
        a->animation.pixels == nullptr || a->animation.coverage == nullptr
            || a->animation.width != 128 || a->animation.height != 64
            || a->animation.count != 5,
        "Invalid Mima animation bank");
    FAIL_IF(
        a->background.width != 640 || a->background.height != 400
            || a->background.pixels == nullptr || !a->background.has_palette,
        "Invalid Mima background");
    MUST(Missiles_ValidateSprites(&a->missiles));
    FAIL_IF(
        a->next_scene.id != 4 || a->next_scene.layouts.stages == nullptr
            || a->pillars == nullptr,
        "Missing Mima scene or pillars");
    return OK;
}

RESULT B10J_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B10J_ASSETS *const out)
{
    *out = (B10J_ASSETS) {};
    B10J_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS3_1.BOS", a, &loaded.still));
    MUST(M_LoadBos(vfs, "BOSS3_2.BOS", a, &loaded.animation));
    MUST(M_LoadBackground(vfs, a, &loaded.background));
    MUST(M_LoadMissiles(vfs, a, &loaded.missiles));
    MUST(Scene_Load(vfs, 4, a, &loaded.next_scene));
    MUST(M_LoadPillars(vfs, scratch, scratch_size, a, &loaded.pillars));
    MUST(B10J_ValidateAssets(&loaded));
    Palette_Import(&loaded.palette, loaded.background.palette);
    *out = loaded;
    return OK;
}

void B10J_Init(B10J *const b, const ALLOCATOR *const a)
{
    *b = (B10J) { .background = Allocator_Alloc(a, 128 * 160),
                  .meteor_active = true,
                  .spread_interval = 4,
                  .spread_speed = 8 };
}

void B10J_BeginSetup(B10J *const b, const B10J_CONTEXT *const c)
{
    ASSERT(!b->setup_active && !b->entrance_active && !b->defeating);
    ASSERT(b->background != nullptr);
    ASSERT(Result_Ignore(B10J_ValidateAssets(c->assets)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->still, c->sprites, 0, &c->assets->still)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->animation, c->sprites, 1, &c->assets->animation)));
    Palette_Set(c->hit->palette, &c->assets->palette);
    b->post_defeat = c->hit->palette->saved;
    b->boss_palette = c->hit->palette->saved;
    b->still.image = 0;
    b->animation.image = 1;
    Palette_BeginFade(&b->setup_fade, c->hit->palette, PALETTE_FADE_WHITE_IN);
    Frame_BeginWait(&b->setup_wait);
    b->setup_active = true;
    b->finished = false;
}

bool B10J_ResumeSetup(B10J *const b, const B10J_CONTEXT *const c)
{
    if (!b->setup_active) {
        return true;
    }
    if (!Frame_WaitReady(&b->setup_wait)) {
        return false;
    }
    Palette_FadeStep(&b->setup_fade, c->hit->palette);
    if (b->setup_fade.remaining != 0) {
        Frame_BeginWait(&b->setup_wait);
        return false;
    }
    BossEntity_SetPosition(
        &b->still, 256, 64, (BOSS_BOUNDS) { 0, 736, 64, 304 });
    BossEntity_SetOrbHitbox(&b->still, (BOSS_BOUNDS) { 16, 112, 16, 144 });
    b->still.orb_inactive = false;
    b->frame = 0;
    b->phase = 0;
    b->hp = 12;
    c->hit->health->first_white = 6;
    c->hit->health->first_redwhite = 2;
    Particles_Init(c->particles);
    Missiles_Reset(c->missiles);
    *c->stage_palette = c->hit->palette->saved;
    b->setup_active = false;
    return true;
}

static PLAYER *M_Player(const B10J_CONTEXT *const c)
{
    return c->hit->display->player;
}

static int16_t M_Rank(
    const B10J_CONTEXT *const c, const int16_t e, const int16_t n,
    const int16_t h, const int16_t l)
{
    switch (c->hit->display->rank) {
    case SETTINGS_DIFFICULTY_EASY:
        return e;
    case SETTINGS_DIFFICULTY_HARD:
        return h;
    case SETTINGS_DIFFICULTY_LUNATIC:
        return l;
    default:
        return n;
    }
}

static void M_Effect(const B10J_CONTEXT *const c, const int16_t effect)
{
    c->hit->effect(c->hit->user, effect);
}

static void M_Snap(B10J *const b, const B10J_CONTEXT *const c)
{
    Video_Read(
        1, (GFX_RECT) { b->still.left, b->still.top, 128, 160 }, b->background,
        128);
    Video_SelectDrawingPage(0);
}

static void M_Erase(const B10J *const b, const B10J_CONTEXT *const c)
{
    const VIDEO_IMAGE bg = { .pixels = b->background,
                             .width = 128,
                             .height = 160,
                             .stride = 128,
                             .key = -1 };
    Video_Blit(&bg, (GFX_RECT) { 0, 0, 128, 160 }, b->still.left, b->still.top);
}

static void M_EraseBoth(const B10J *const b, const B10J_CONTEXT *const c)
{
    Video_SelectDrawingPage(1);
    M_Erase(b, c);
    Video_SelectDrawingPage(0);
    M_Erase(b, c);
}

static void M_PutStill(const B10J *const b, const B10J_CONTEXT *const c)
{
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossEntity_Put(
            &b->still, c->sprites, b->still.left, b->still.top, b->still.image);
    }
}

static void M_AnimationBoth(
    B10J *const b, const B10J_CONTEXT *const c, const int16_t image)
{
    b->animation.left = b->still.left;
    b->animation.top = Math16_Wrap(b->still.top + 48);
    b->animation.image = image;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(page);
        BossEntity_Put(
            &b->animation, c->sprites, b->animation.left, b->animation.top,
            image);
    }
}

static void M_Cast(B10J *const b, const B10J_CONTEXT *const c)
{
    b->meteor_active = false;
    M_AnimationBoth(b, c, 0);
}

static void M_Activate(B10J *const b, const B10J_CONTEXT *const c)
{
    if (!b->meteor_active) {
        b->meteor_active = true;
        M_AnimationBoth(b, c, 1);
    }
}

static void M_Meteor(B10J *const b, const B10J_CONTEXT *const c)
{
    if (b->meteor_active && b->frame % 8 == 0) {
        b->animation.left = b->still.left;
        b->animation.top = Math16_Wrap(b->still.top + 48);
        b->animation.image = (int16_t)(1 + (b->frame / 8) % 4);
        BossEntity_RestorePut(
            &b->animation, c->sprites, b->animation.left, b->animation.top,
            b->animation.image);
    }
}

static void M_Spread(
    B10J *const b, const B10J_CONTEXT *const c, const int16_t left,
    const int16_t top)
{
    if (b->frame < 10) {
        return;
    }
    if (b->frame == 10) {
        b->still.left = left;
        b->still.top = top;
        M_Snap(b, c);
        return;
    }
    if (b->frame % b->spread_interval != 0) {
        return;
    }
    const int16_t bottom =
        Math16_Wrap((b->spread_speed / b->spread_interval) * (b->frame - 10));
    const int16_t start = Math16_Wrap(80 - bottom);
    if (start < 0) {
        b->frame = 0;
        M_PutStill(b, c);
        return;
    }
    for (int16_t row = 0; row < bottom; row++) {
        BossEntity_RestorePutLine(
            &b->still, c->sprites, left, Math16_Wrap(top + start + row),
            b->still.image, row);
        BossEntity_RestorePutLine(
            &b->still, c->sprites, left, Math16_Wrap(top + 160 - start - row),
            b->still.image, (int16_t)(159 - row));
    }
}

static void M_BrokenTransition(B10J *const b, const B10J_CONTEXT *const c)
{
    if (b->frame < 10 || b->frame % 4 != 0) {
        return;
    }
    const int16_t half = Math16_Wrap((b->frame - 10) * 2);
    if (half >= 80) {
        b->frame = 0;
        return;
    }
    Sprite_Restore16(
        b->still.left, Math16_Wrap(b->still.top + half),
        b->still.sprites.width_bytes, 8);
    Sprite_Restore16(
        b->still.left, Math16_Wrap(b->still.top + 152 - half),
        b->still.sprites.width_bytes, 8);
}

static void M_Corners(
    const B10J *const b, int16_t x[4], int16_t y[4], const int16_t radius,
    const uint8_t angle)
{
    for (int32_t i = 0; i < 4; i++) {
        x[i] = Math16_PolarX(
            Math16_Wrap(b->still.left + 64), radius, angle + i * 64);
        y[i] = Math16_PolarY(
            Math16_Wrap(b->still.top + 80), radius, angle + i * 64);
    }
}

static void M_Loop(
    const B10J_CONTEXT *const c, const int16_t x[4], const int16_t y[4],
    const bool restore)
{
    for (int32_t i = 0; i < 4; i++) {
        const int32_t next = (i + 1) % 4;
        if (restore) {
            Video_LineErase(x[i], y[i], x[next], y[next]);
        } else {
            Video_Line(x[i], y[i], x[next], y[next], 7);
        }
    }
}

static void M_Add(
    const B10J_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed, const bool chase)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        chase ? PELLET_CHASE : PELLET_REGULAR, chase ? speed : 0, 0, 0);
}

static void M_SquareInit(B10J_SQUARE *const sq)
{
    sq->angle = 0;
    sq->radius = 32;
}

static void M_AimedStatic(B10J *const b, const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &b->squares[0];
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SquareInit(sq);
        b->pattern_state = M_Rank(c, 64, 72, 80, 88);
        M_Effect(c, 8);
    }
    int16_t x[4], y[4];
    if (b->frame % 8 == 0) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle = (uint8_t)(sq->angle + (b->frame < 260 ? 12 : -12));
        if (sq->radius < 112) {
            sq->radius += 8;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                const uint8_t angle = b->frame > 280
                    ? Math16_Angle(
                          Math16_Wrap(y[i] - b->still.top - 80),
                          Math16_Wrap(x[i] - b->still.left - 64))
                    : Math16_Angle(
                          Math16_Wrap(384 - y[i]),
                          Math16_Wrap(M_Player(c)->left + 12 - x[i]));
                M_Add(
                    c, x[i], y[i], angle,
                    b->frame > 280 ? b->pattern_state
                                   : (int16_t)(b->pattern_state / 2),
                    false);
                M_Effect(c, 7);
            }
        }
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (b->frame > 360) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        b->frame = 0;
    }
}

static void M_AimedMissiles(B10J *const b, const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &b->squares[1];
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SquareInit(sq);
        b->pattern_state = M_Rank(c, 96, 104, 112, 118);
        M_Effect(c, 8);
    }
    int16_t x[4], y[4];
    if (b->frame % 8 == 0) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle -= 12;
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (b->frame == 224) {
            b->target_left = M_Player(c)->left;
        } else if (b->frame > 240) {
            const MATH16_VECTOR velocity = Math16_Aim(
                (MATH16_VECTOR) { Math16_Wrap(b->still.left + 64),
                                  Math16_Wrap(b->still.top + 80) },
                (MATH16_VECTOR) { Math16_Wrap(b->target_left + 8), 384 },
                b->pattern_state, 0);
            for (int32_t i = 0; i < 4; i++) {
                Missiles_Add(
                    c->missiles, x[i], y[i], Math16_ToPixel(velocity.x),
                    Math16_ToPixel(velocity.y), 0);
            }
            M_Effect(c, 6);
        }
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (b->frame > 320) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        b->frame = 0;
    }
}

static void M_TwoSquares(B10J *const b, const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &b->squares[2];
    if (b->frame == 50) {
        M_Cast(b, c);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SquareInit(sq);
        b->pattern_state = M_Rank(c, 64, 72, 80, 88);
        M_Effect(c, 8);
    }
    int16_t x[4], y[4], x2[4], y2[4];
    if (b->frame % 8 == 0) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Corners(b, x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(c, x, y, true);
        M_Loop(c, x2, y2, true);
        sq->angle -= 6;
        c->pellets->spawn_with_cloud = true;
        if (sq->radius < 112) {
            sq->radius += 8;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Add(
                    c, x[i], y[i],
                    Math16_Angle(
                        Math16_Wrap(y[i] - b->still.top - 80),
                        Math16_Wrap(x[i] - b->still.left - 64)),
                    b->pattern_state, false);
                M_Add(
                    c, x2[i], y2[i],
                    Math16_Angle(
                        Math16_Wrap(y2[i] - b->still.top - 80),
                        Math16_Wrap(x2[i] - b->still.left - 64)),
                    b->pattern_state, false);
                M_Effect(c, 7);
            }
        }
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Corners(b, x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(c, x, y, false);
        M_Loop(c, x2, y2, false);
        c->pellets->spawn_with_cloud = false;
    }
    if (b->frame > 320) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Corners(b, x2, y2, sq->radius, (uint8_t)-sq->angle);
        M_Loop(c, x, y, true);
        M_Loop(c, x2, y2, true);
        b->frame = 0;
        M_Activate(b, c);
    }
}

static void M_InitHop(B10J *const b, const B10J_CONTEXT *const c)
{
    b->hop = 255;
    b->spread_interval = 4;
    b->direction = (uint8_t)(MasterRng_Next(c->rng) % 2);
    b->spread_speed = 16;
}

static void M_Hop(B10J *const b, const B10J_CONTEXT *const c)
{
    if (b->hop == 255) {
        M_EraseBoth(b, c);
        b->meteor_active = false;
        b->hop = 0;
        b->frame = 1;
    }
    if (b->hop == 0) {
        b->still.orb_inactive = true;
        M_BrokenTransition(b, c);
    } else {
        if (b->frame == 4) {
            b->still.left = b->direction == 0 ? (int16_t)((b->hop - 1) * 128)
                                              : (int16_t)(640 - b->hop * 128);
            b->still.top = 96;
            M_Snap(b, c);
        }
        b->still.orb_inactive = b->frame < 12;
        M_Spread(b, c, b->still.left, b->still.top);
    }
    if (b->frame != 0) {
        return;
    }
    if (b->hop != 0) {
        if (b->hop != 4) {
            M_EraseBoth(b, c);
        }
        b->pattern_state = M_Rank(c, 30, 35, 40, 45);
        const int16_t count = M_Rank(c, 5, 8, 10, 12);
        uint8_t angle = 0;
        for (int16_t i = 0; i < count;
             i++, angle = (uint8_t)(angle + 256 / count)) {
            M_Add(
                c, Math16_Wrap(b->still.left + 64),
                Math16_Wrap(b->still.top + 80), angle, b->pattern_state, true);
        }
    }
    if (b->hop >= 4) {
        M_PutStill(b, c);
        Palette_Set(c->hit->palette, c->stage_palette);
        b->hop = 255;
        b->direction = (uint8_t)(1 - b->direction);
        b->meteor_active = true;
        b->frame = 0;
    } else {
        for (int32_t i = 0; i < 16; i++) {
            const int8_t *const color = c->hit->palette->saved.colors[i];
            Palette_SetColor(
                c->hit->palette, i, color[0] - 4, color[1] - 4, color[2] - 4);
        }
        b->hop++;
        b->frame = 1;
    }
}

static void M_Pillar(
    const B10J_CONTEXT *const c, const int16_t left, const int16_t bottom,
    const int16_t height)
{
    const int32_t segments = height > 8 ? height - 8 : 0;
    uint16_t offset = (uint16_t)(bottom * 80 + left / 8 - (left % 8 < 0));
    for (int32_t row = 0; row < segments + 8; row++) {
        const int32_t part = row < segments ? 0 : 1;
        const int32_t index = 7 - ((row < segments ? row : row - segments) % 8);
        const uint8_t *const mask = c->assets->pillars->masks[part][index];
        for (int32_t byte = 0; byte < 4; byte++) {
            const uint32_t address = (offset + (uint32_t)byte) & 0x7fffu;
            if (address >= 32000) {
                continue;
            }
            for (int32_t bit = 0; bit < 8; bit++) {
                if ((mask[byte] & (0x80u >> bit)) != 0) {
                    g_Video.pages[g_Video.drawing_page][address * 8 + bit] = 10;
                }
            }
        }
        offset = (uint16_t)(offset - 80);
    }
}

static void M_Pillars(B10J *const b, const B10J_CONTEXT *const c)
{
    if (b->frame == 50) {
        M_Cast(b, c);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        for (int32_t i = 0; i < 8; i++) {
            b->pillar_time[i] = 128;
            b->pillar_bottom[i] = 400;
        }
        b->pillar_x[0] = (int16_t)(16 + MasterRng_Next(c->rng) % 48);
        b->pillar_x[1] = (int16_t)(624 - MasterRng_Next(c->rng) % 48);
        b->pillar_x[2] = (int16_t)(64 + MasterRng_Next(c->rng) % 224);
        b->pillar_x[3] = (int16_t)(336 + MasterRng_Next(c->rng) % 224);
        b->pillar_x[4] = (int16_t)(128 + MasterRng_Next(c->rng) % 160);
        b->pillar_x[5] = (int16_t)(336 + MasterRng_Next(c->rng) % 160);
        b->pillar_x[6] = (int16_t)(64 + MasterRng_Next(c->rng) % 224);
        b->pillar_x[7] = (int16_t)(336 + MasterRng_Next(c->rng) % 224);
        b->pattern_state = M_Rank(c, 40, 45, 50, 55);
        M_Effect(c, 8);
    }
    for (int32_t i = 0; i < 8; i++) {
        if (b->frame % 8 == i && b->frame > 100 + i * 20
            && b->pillar_time[i] > 32) {
            Shape_EllipseArcErase(
                b->pillar_x[i], b->pillar_bottom[i], b->pillar_time[i],
                b->pillar_time[i], 4, 0, 255);
            b->pillar_time[i] -= 8;
        }
    }
    if (b->frame <= 240 && (b->frame - 100) % 20 == 0) {
        M_Effect(c, 12);
    }
    if (b->frame == 240) {
        M_Activate(b, c);
    }
    for (int32_t i = 0; i < 8; i++) {
        if (b->frame % 8 != i || b->frame <= 100 + i * 20) {
            continue;
        }
        const int16_t x = b->pillar_x[i], bottom = b->pillar_bottom[i];
        if (b->pillar_time[i] > 32) {
            Shape_EllipseArcPut(
                x, bottom, b->pillar_time[i], b->pillar_time[i], 7, 4, 0, 255);
            Video_Line(320, 200, x, bottom, 1);
        } else if (b->pillar_time[i] > 16) {
            if (b->pillar_time[i] == 32) {
                M_Effect(c, 7);
                Video_LineErase(320, 200, x, bottom);
            }
            M_Pillar(
                c, Math16_Wrap(x - 16), bottom,
                Math16_Wrap(16 * (34 - b->pillar_time[i])));
            if (!M_Player(c)->invincible
                && M_Player(c)->left > Math16_Wrap(x - 40)
                && M_Player(c)->left < Math16_Wrap(x + 8)) {
                M_Player(c)->is_hit = true;
            }
            b->pillar_time[i] -= 8;
        } else if (b->pillar_time[i] != -999) {
            Pellets_AddGroup(
                c->pellets, c->pellet_shared, M_Player(c), c->rng,
                c->hit->display->rank, Math16_Wrap(b->still.left + 64),
                Math16_Wrap(b->still.top + 80), PELLET_GROUP_THREE_WIDE_AIMED,
                36);
            Sprite_Restore16(Math16_Wrap(x - 16), 176, 32, 224);
            b->pillar_time[i] = -999;
        }
    }
    bool done = true;
    for (int32_t i = 0; i < 8; i++) {
        done = done && b->pillar_time[i] == -999;
    }
    if (done) {
        b->frame = 0;
    }
}

static void M_Halfcircle(B10J *const b, const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &b->squares[3];
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SquareInit(sq);
        b->missile_angle = 0;
        b->pattern_state = M_Rank(c, 40, 45, 50, 55);
        M_Effect(c, 8);
    }
    int16_t x[4], y[4];
    if (b->frame % 8 == 0) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle -= 12;
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (b->frame > 180 && b->frame % 16 == 8) {
            const MATH16_VECTOR velocity = Math16_Vector(
                (int16_t)(b->pattern_state / 10), b->missile_angle);
            for (int32_t i = 0; i < 4; i++) {
                Missiles_Add(
                    c->missiles, x[i], y[i], velocity.x, velocity.y, 0);
            }
            M_Effect(c, 6);
            b->missile_angle += 13;
        }
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (b->frame > 340) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        b->frame = 0;
    }
}

static void M_Spray(B10J *const b, const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &b->squares[4];
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SquareInit(sq);
        b->pellet_angle = 128;
        b->pattern_state = M_Rank(c, 32, 40, 48, 56);
        M_Effect(c, 8);
    }
    int16_t x[4], y[4];
    if (b->frame % 8 == 0) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        sq->angle = (uint8_t)(sq->angle + (b->frame > 270 ? 12 : -12));
        if (sq->radius < 80) {
            sq->radius += 8;
        } else if (b->frame > 180) {
            b->pellet_angle =
                (uint8_t)(b->pellet_angle + (b->frame > 270 ? 12 : -12));
            for (int32_t i = 0; i < 4; i++) {
                M_Add(c, x[i], y[i], b->pellet_angle, b->pattern_state, false);
            }
            M_Effect(c, 6);
        }
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, false);
    }
    if (b->frame > 370) {
        M_Corners(b, x, y, sq->radius, sq->angle);
        M_Loop(c, x, y, true);
        b->frame = 0;
    }
}

static void M_Lasers(B10J *const b, const B10J_CONTEXT *const c)
{
    B10J_SQUARE *const sq = &b->squares[5];
    if (b->frame == 50) {
        M_Cast(b, c);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        M_SquareInit(sq);
        b->pattern_state = M_Rank(c, 50, 54, 58, 62);
        M_Effect(c, 8);
    }
    if (b->frame % 8 == 0) {
        M_Corners(b, b->laser_x, b->laser_y, sq->radius, sq->angle);
        M_Loop(c, b->laser_x, b->laser_y, true);
        sq->angle += 3;
        if (sq->radius < 80) {
            sq->radius += 8;
        }
        M_Corners(b, b->laser_x, b->laser_y, sq->radius, sq->angle);
        M_Loop(c, b->laser_x, b->laser_y, false);
    }
    if (b->frame > 180 && b->frame < 300) {
        const int32_t i = b->frame % 4;
        ShootoutLasers_Spawn(
            &c->lasers->slots[i], b->laser_x[i], b->laser_y[i],
            Math16_Wrap(M_Player(c)->left + 14), 400, b->pattern_state, 7, 20,
            4);
    }
    if (b->frame > 300) {
        M_Corners(b, b->laser_x, b->laser_y, sq->radius, sq->angle);
        M_Loop(c, b->laser_x, b->laser_y, true);
        b->frame = 0;
    }
}

static void M_Hit(B10J *const b, const B10J_CONTEXT *const c)
{
    BOSS_HIT_CONTEXT hit = *c->hit;
    hit.hp = &b->hp;
    hit.boss_palette = &b->boss_palette;
    const uint8_t flash[] = { 3, 9 };
    BossHit_UpdateAndRender(
        &b->hit, &hit, flash, 2, 5000,
        BossEntity_HitTestOrb(&b->still, c->hit->orb),
        (BOSS_SHOT_HITBOX) { Math16_Wrap(b->still.left + 16),
                             Math16_Wrap(b->still.top + 48), 96, 48 });
}

static void M_MissileAction(void *const user, const MISSILES_ACTION action)
{
    const B10J_CONTEXT *const c = user;
    if (action.kind == MISSILES_ACTION_EFFECT) {
        M_Effect(c, action.effect);
    } else {
        Missiles_RenderAction(&c->assets->missiles, action);
    }
}

static bool M_Entrance(B10J *const b, const B10J_CONTEXT *const c)
{
    for (;;) {
        if (b->entrance_waiting) {
            if (!Frame_DelayReady(2)) {
                return false;
            }
            b->entrance_waiting = false;
        }
        b->frame = Math16_Wrap(b->frame + 1);
        M_Spread(b, c, 256, 120);
        if (b->frame == 0) {
            b->phase = 1;
            b->pattern = 0;
            b->hp_bar_filled = false;
            *c->stage_palette = c->hit->palette->saved;
            b->boss_palette = c->hit->palette->saved;
            M_InitHop(b, c);
            b->entrance_active = false;
            return true;
        }
        b->entrance_waiting = true;
    }
}

static bool M_Death(B10J *const b, const B10J_CONTEXT *const c)
{
    const BOSS_DEFEAT_CONTEXT defeat = { .palette = c->hit->palette,
                                         .post_defeat = &b->post_defeat,
                                         .miko = c->miko,
                                         .player = M_Player(c),
                                         .origin_top = &c->hit->orb->top,
                                         .stage_cleared = c->stage_cleared };
    if (!BossDefeat_Resume(&b->defeat, &defeat)) {
        return false;
    }
    Scene_Activate(c->scene, &c->assets->next_scene);
    b->defeating = false;
    b->finished = true;
    return true;
}

RESULT B10J_ValidateContext(const B10J_CONTEXT *const c)
{
    FAIL_IF(
        c == nullptr || c->assets == nullptr || c->sprites == nullptr
            || c->hit == nullptr || c->hit->display == nullptr
            || c->hit->palette == nullptr || c->hit->health == nullptr
            || c->hit->hud == nullptr || c->hit->shots == nullptr
            || c->hit->shots_output.action == nullptr || c->hit->orb == nullptr
            || c->hit->test_damage == nullptr || c->hit->effect == nullptr
            || c->hit->display->remaining_lives == nullptr
            || c->hit->display->high_score == nullptr
            || c->hit->display->font == nullptr
            || c->hit->display->sprites == nullptr
            || c->hit->display->player == nullptr
            || c->hit->display->gameplay == nullptr
            || c->stage_palette == nullptr || c->particles == nullptr
            || c->pellets == nullptr || c->pellet_shared == nullptr
            || c->missiles == nullptr || c->lasers == nullptr
            || c->rng == nullptr || c->miko == nullptr || c->scene == nullptr
            || c->stage_cleared == nullptr || c->fade_music == nullptr,
        "Missing Mima context");
    MUST(B10J_ValidateAssets(c->assets));
    FAIL_IF(
        c->hit->health->backgrounds == nullptr
            || c->hit->hud->numbers == nullptr,
        "Missing Mima HUD storage");
    MUST(Shots_ValidateSprites(c->hit->display->sprites));
    for (uint32_t digit = 0xff10; digit <= 0xff19; digit++) {
        FAIL_IF(
            GameText_Find(c->hit->display->font, digit) == nullptr,
            "Missing Mima score glyph");
    }
    MUST(BossDefeat_ValidateSprites(c->miko));
    return OK;
}

bool B10J_UpdateAndRender(B10J *const b, const B10J_CONTEXT *const c)
{
    ASSERT(!b->setup_active);
    if (b->finished) {
        return true;
    }
    if (b->entrance_active) {
        return M_Entrance(b, c);
    }
    if (b->defeating) {
        return M_Death(b, c);
    }
    Missiles_UpdateAndRender(
        c->missiles, M_Player(c), (uint16_t)c->hit->frame_rand,
        (MISSILES_OUTPUT) { M_MissileAction, (void *)c });
    Particles_UpdateAndRender(c->particles, c->rng, PARTICLES_TOP_RIGHT, 7);
    if (b->phase == 0) {
        Video_SelectDrawingPage(0);
        b->frame = 0;
        b->still.orb_inactive = false;
        b->spread_interval = 1;
        b->spread_speed = 2;
        b->meteor_active = true;
        b->entrance_active = true;
        return M_Entrance(b, c);
    }
    if (b->phase == 1) {
        if (!b->hp_bar_filled) {
            b->hp_bar_filled = HudHp_Increment(c->hit->health, b->hp, b->frame);
        }
        b->frame = Math16_Wrap(b->frame + 1);
        b->hit.frame = Math16_Wrap(b->hit.frame + 1);
        M_Meteor(b, c);
        switch (b->pattern) {
        case 0:
            M_AimedStatic(b, c);
            break;
        case 1:
            M_AimedMissiles(b, c);
            break;
        case 2:
            M_TwoSquares(b, c);
            break;
        case 3:
            M_Hop(b, c);
            break;
        default:
            break;
        }
        if (b->frame == 0) {
            b->pattern = b->pattern == 3 ? 0 : Math16_Wrap(b->pattern + 1);
            if (b->hp <= 6) {
                b->pattern = 99;
            }
        }
        M_Hit(b, c);
        if (b->pattern == 99 && !b->hit.invincible) {
            M_EraseBoth(b, c);
            b->spread_interval = 4;
            b->spread_speed = 8;
            b->still.left = 256;
            b->still.top = 120;
            M_Snap(b, c);
            b->phase = 2;
            b->frame = 0;
            b->hit.frame = 0;
            b->pattern = 0;
        }
    } else if (b->phase == 2) {
        b->frame = Math16_Wrap(b->frame + 1);
        b->hit.frame = Math16_Wrap(b->hit.frame + 1);
        M_Spread(b, c, 256, 120);
        if (b->frame == 0) {
            b->phase = 3;
            b->frame = 0;
            b->hit.frame = 0;
            b->pattern = 0;
        }
    } else if (b->phase == 3) {
        b->frame = Math16_Wrap(b->frame + 1);
        b->hit.frame = Math16_Wrap(b->hit.frame + 1);
        M_Meteor(b, c);
        switch (b->pattern) {
        case 0:
            M_Pillars(b, c);
            break;
        case 1:
            M_Halfcircle(b, c);
            break;
        case 2:
            M_Spray(b, c);
            break;
        case 3:
            M_Lasers(b, c);
            break;
        default:
            break;
        }
        if (b->frame == 0) {
            b->pattern = b->pattern == 3 ? 0 : Math16_Wrap(b->pattern + 1);
        }
        M_Hit(b, c);
        if (b->hp <= 0) {
            Video_SelectDrawingPage(1);
            M_Erase(b, c);
            Video_SelectDrawingPage(0);
            c->fade_music(c->hit->user);
            Pellets_EraseAndResetMoving(c->pellets);
            Missiles_Reset(c->missiles);
            ShootoutLasers_EraseAndReset(c->lasers, 5);
            BossDefeat_Begin(&b->defeat, BOSS_DEFEAT_REGULAR);
            b->defeating = true;
            return M_Death(b, c);
        }
    }
    return true;
}
