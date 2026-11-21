#include "th01/boss_b20j.h"

#include "core/base/assert.h"
#include "th01/sprite.h"

#include <string.h>

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
static RESULT M_LoadPi(
    VFS *const vfs, const char *const name, const ALLOCATOR *const a,
    PI_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &reader));
    const RESULT result = Pi_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadBody(
    VFS *const vfs, const ALLOCATOR *const a, GRZ *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "BOSS8.GRZ", VFS_IN_MEMORY, &reader));
    const RESULT result = Grz_Load(&reader, a, out);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadAudio(
    VFS *const vfs, const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "ALICE.MDT", VFS_IN_MEMORY, &reader));
    RESULT result = MdrvData_LoadMusic(&reader, a, &out->music);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "ZIGOKU.MDE", VFS_IN_MEMORY, &reader));
    result = MdrvData_LoadEffects(&reader, a, &out->effects);
    Reader_Close(&reader);
    return result;
}
static RESULT M_LoadMasks(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    READER image;
    RESULT result = ReiidenImage_Open(&reader, scratch, scratch_size, &image);
    uint8_t masks[24];
    if (IS_OK(result)) {
        Reader_Seek(&image, 0x3924a);
        Reader_ReadBytes(&image, masks, sizeof(masks));
        result = Reader_Result(&image);
    }
    Reader_Close(&reader);
    MUST(result);
    uint8_t *const allocated = Allocator_Alloc(a, 16);
    memcpy(allocated, masks, 8);
    memcpy(allocated + 8, masks + 16, 8);
    out->diamond = allocated;
    out->flake = allocated + 8;
    return OK;
}
RESULT B20J_ValidateAssets(const B20J_ASSETS *const a)
{
    FAIL_IF(a == nullptr, "Missing Konngara assets");
    const BOS *const banks[] = { &a->head, &a->face, &a->aim };
    const int32_t counts[] = { 3, 6, 3 };
    const int32_t heights[] = { 80, 32, 32 };
    for (int32_t i = 0; i < 3; i++) {
        FAIL_IF(
            banks[i]->pixels == nullptr || banks[i]->coverage == nullptr
                || banks[i]->count != counts[i] || banks[i]->width != 80
                || banks[i]->height != heights[i],
            "Invalid Konngara sprite bank");
    }
    FAIL_IF(a->body.count != 16, "Missing Konngara body images");
    for (int32_t i = 0; i < 16; i++) {
        const GRX *const image = &a->body.images[i];
        FAIL_IF(
            image->stream_count < 1 || image->pixels == nullptr
                || image->coverage == nullptr,
            "Invalid Konngara body image");
    }
    const PI_IMAGE *const images[] = { &a->scroll,    &a->upper,
                                       &a->layers[0], &a->layers[1],
                                       &a->layers[2], &a->layers[3] };
    for (int32_t i = 0; i < 6; i++) {
        FAIL_IF(
            images[i]->pixels == nullptr || images[i]->width <= 0
                || images[i]->height <= 0 || !images[i]->has_palette,
            "Invalid Konngara background");
    }
    FAIL_IF(
        a->music.bytes == nullptr || a->effects.bytes == nullptr
            || a->diamond == nullptr || a->flake == nullptr,
        "Missing Konngara audio or shape masks");
    return OK;
}
RESULT B20J_LoadAssets(
    VFS *const vfs, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const a, B20J_ASSETS *const out)
{
    *out = (B20J_ASSETS) {};
    B20J_ASSETS loaded = {};
    MUST(M_LoadBos(vfs, "BOSS8_1.BOS", a, &loaded.head));
    MUST(M_LoadBos(vfs, "BOSS8_E1.BOS", a, &loaded.face));
    MUST(M_LoadBos(vfs, "BOSS8_E2.BOS", a, &loaded.aim));
    MUST(M_LoadBody(vfs, a, &loaded.body));
    MUST(M_LoadPi(vfs, "BOSS7_D1.GRP", a, &loaded.scroll));
    MUST(M_LoadPi(vfs, "BOSS8_A1.GRP", a, &loaded.upper));
    const char *const names[] = { "BOSS8_D1.GRP", "BOSS8_D2.GRP",
                                  "BOSS8_D3.GRP", "BOSS8_D4.GRP" };
    for (int32_t i = 0; i < 4; i++) {
        MUST(M_LoadPi(vfs, names[i], a, &loaded.layers[i]));
    }
    MUST(M_LoadAudio(vfs, a, &loaded));
    MUST(M_LoadMasks(vfs, scratch, scratch_size, a, &loaded));
    MUST(B20J_ValidateAssets(&loaded));
    *out = loaded;
    return OK;
}
void B20J_Init(B20J *const b)
{
    *b = (B20J) { .ray_previous_x = 999, .ray_previous_y = 999 };
}
void B20J_Setup(B20J *const b, const B20J_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && !b->defeating && b->wait_kind == 0);
    ASSERT(Result_Ignore(B20J_ValidateAssets(c->assets)));
    b->boss_palette = c->hit->palette->saved;
    b->hp = 18;
    b->phase = 0;
    b->frame = 0;
    b->direction_can_change = true;
    b->face_expression = 0;
    b->face_direction = 2;
    b->finished = false;
    c->hit->health->first_white = 16;
    c->hit->health->first_redwhite = 10;
}

static VIDEO *M_Video(const B20J_CONTEXT *const c)
{
    return c->hit->display->video;
}
static PLAYER *M_Player(const B20J_CONTEXT *const c)
{
    return c->hit->display->player;
}
static int16_t M_W(const int32_t x)
{
    return Math_Wrap16(x);
}
static int16_t M_Rank(
    const B20J_CONTEXT *const c, const int16_t e, const int16_t n,
    const int16_t h, const int16_t l)
{
    const int32_t rank = c->hit->display->rank;
    return rank == 0 ? e : rank == 2 ? h : rank == 3 ? l : n;
}
static uint16_t M_Random(const B20J_CONTEXT *const c)
{
    return Math_Random(c->rng);
}
static void M_Effect(const B20J_CONTEXT *const c, const int16_t effect)
{
    c->hit->effect(c->hit->user, effect);
}
static void M_Audio(const B20J_CONTEXT *const c, const B20J_AUDIO_ACTION action)
{
    c->audio(c->hit->user, action, c->assets);
}
static void M_Pi(
    const B20J_CONTEXT *const c, const PI_IMAGE *const image,
    const bool colorkey)
{
    const VIDEO_IMAGE view = { .pixels = image->pixels,
                               .width = image->width,
                               .height = image->height,
                               .stride = image->width,
                               .key = colorkey ? 15 : -1 };
    Video_Blit(
        M_Video(c), &view, (GFX_RECT) { 0, 0, image->width, image->height }, 0,
        0);
    Palette_Import(&c->hit->palette->resource, image->palette);
    if (!colorkey) {
        Palette_Set(c->hit->palette, M_Video(c), &c->hit->palette->resource);
    }
}
static void M_Grx(const B20J_CONTEXT *const c, const int16_t image)
{
    const GRX *const grx = &c->assets->body.images[image];
    const VIDEO_IMAGE view = { .pixels = grx->pixels,
                               .coverage = grx->coverage,
                               .width = 640,
                               .height = 400,
                               .stride = 640,
                               .coverage_stride = 640,
                               .key = -1 };
    Video_Blit(M_Video(c), &view, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0);
}
static void M_Head(
    const B20J *const b, const B20J_CONTEXT *const c, const int16_t direction)
{
    BossEntity_Put(&b->head, c->sprites, M_Video(c), 280, 80, direction);
}
static void M_Face(
    const B20J *const b, const B20J_CONTEXT *const c, const int16_t expression,
    const int16_t direction)
{
    if (expression == 3) {
        BossEntity_Put(&b->aim, c->sprites, M_Video(c), 280, 128, direction);
    } else if (expression != 0) {
        BossEntity_Put(
            &b->face, c->sprites, M_Video(c), 280, 128,
            (expression - 1) * 3 + direction);
    }
}
static void M_Direction(
    B20J *const b, const B20J_CONTEXT *const c, const int16_t direction)
{
    if (!b->direction_can_change || b->face_direction == direction) {
        return;
    }
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(M_Video(c), page);
        M_Head(b, c, direction);
    }
    if (b->face_expression != 0) {
        for (int32_t page = 1; page >= 0; page--) {
            Video_SelectDrawingPage(M_Video(c), page);
            M_Face(b, c, b->face_expression, direction);
        }
    }
    b->face_direction = direction;
}
static void M_Expression(
    B20J *const b, const B20J_CONTEXT *const c, const int16_t expression)
{
    if (b->face_expression == expression) {
        return;
    }
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(M_Video(c), page);
        if (expression == 0) {
            M_Head(b, c, b->face_direction);
        } else {
            M_Face(b, c, expression, b->face_direction);
        }
    }
    b->face_expression = expression;
}
static void M_SlashPut(const B20J_CONTEXT *const c, const int16_t image)
{
    Video_SelectDrawingPage(M_Video(c), 1);
    M_Grx(c, image);
    Video_SelectDrawingPage(M_Video(c), 0);
    M_Grx(c, image);
}
static void M_White(const B20J_CONTEXT *const c)
{
    Palette_SetColor(c->hit->palette, M_Video(c), 9, 15, 15, 15);
}
static void M_WhiteIn(const B20J_CONTEXT *const c)
{
    int8_t *const col = c->hit->palette->saved.colors[9];
    if (col[0] > 0) {
        col[0]--;
    }
    if (col[1] > 9) {
        col[1]--;
    }
    if (col[2] > 10) {
        col[2]--;
    }
    Palette_Set(c->hit->palette, M_Video(c), &c->hit->palette->saved);
}
static void M_BlackOut(const B20J_CONTEXT *const c)
{
    PALETTE *const colors = &c->hit->palette->saved;
    for (int32_t i = 0; i < 16; i++) {
        for (int32_t j = 0; j < 3; j++) {
            if (colors->colors[i][j] > 0) {
                colors->colors[i][j]--;
            }
        }
    }
    Palette_Set(c->hit->palette, M_Video(c), colors);
}
static bool M_Scroll(B20J *const b, FRAME_VSYNC *const vsync, const int16_t top)
{
    b->raw_scroll = top;
    b->wait_kind = 1;
    Frame_BeginWait(&b->wait, vsync);
    return false;
}
static bool M_Delay(
    B20J *const b, FRAME_VSYNC *const vsync, const uint16_t frames)
{
    b->delay_frames = frames;
    if (Frame_DelayReady(vsync, frames)) {
        return true;
    }
    b->wait_kind = 2;
    return false;
}
static bool M_Ready(
    B20J *const b, const B20J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (b->wait_kind == 1) {
        if (!Frame_WaitReady(&b->wait, vsync)) {
            return false;
        }
        Video_SetScroll(M_Video(c), b->raw_scroll);
    } else if (b->wait_kind == 2 && !Frame_DelayReady(vsync, b->delay_frames)) {
        return false;
    }
    b->wait_kind = 0;
    return true;
}
void B20J_BeginEntrance(B20J *const b, const B20J_CONTEXT *const c)
{
    ASSERT(!b->entrance_active && !b->defeating && b->wait_kind == 0);
    c->pellets->interlace = true;
    Tram_Fill(c->tram, (GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
    M_Pi(c, &c->assets->scroll, false);
    b->stage_palette = c->hit->palette->saved;
    StageObjects_Init(
        c->objects, nullptr, 19, false, nullptr, M_Video(c), nullptr, c->rng);
    Video_SelectDrawingPage(M_Video(c), 1);
    M_Pi(c, &c->assets->upper, false);
    Video_SelectDrawingPage(M_Video(c), 0);
    M_Audio(c, B20J_AUDIO_LOAD_MUSIC);
    M_Audio(c, B20J_AUDIO_LOAD_EFFECTS);
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->head, c->sprites, 0, &c->assets->head)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->face, c->sprites, 1, &c->assets->face)));
    ASSERT(Result_Ignore(
        BossEntity_Bind(&b->aim, c->sprites, 2, &c->assets->aim)));
    M_Audio(c, B20J_AUDIO_PLAY);
    Palette_SetGray(c->hit->palette, M_Video(c), 0);
    Tram_Clear(c->tram);
    b->scroll_line = 0;
    b->scroll_speed = 32;
    b->scroll_quarter = 0;
    b->scroll_frame = 0;
    b->entrance_stage = 1;
    b->entrance_active = true;
}
bool B20J_ResumeEntrance(
    B20J *const b, const B20J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (!b->entrance_active) {
        return true;
    }
    if (!M_Ready(b, c, vsync)) {
        return false;
    }
    for (;;) {
        switch (b->entrance_stage) {
        case 1:
            b->entrance_stage = 2;
            return M_Scroll(b, vsync, b->scroll_line);
        case 2:
            b->scroll_line += b->scroll_speed;
            for (int32_t quarter = 0; quarter < 4; quarter++) {
                if (b->scroll_quarter == quarter
                    && b->scroll_line > (quarter + 1) * 100) {
                    b->scroll_quarter++;
                    b->scroll_speed--;
                    if (quarter == 3) {
                        b->scroll_quarter = 0;
                        b->scroll_line -= 400;
                    }
                }
            }
            if (b->scroll_speed <= 0) {
                b->entrance_stage = 3;
                continue;
            }
            if (b->scroll_frame % 8 == 0) {
                Palette_StepTo(
                    c->hit->palette, M_Video(c), &c->hit->palette->resource,
                    true);
            }
            b->scroll_frame++;
            b->entrance_stage = 1;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 3:
            b->entrance_stage = 4;
            return M_Scroll(b, vsync, 0);
        case 4:
            b->animation_frame = 0;
            b->entrance_stage = 5;
            if (!M_Delay(b, vsync, 40)) {
                return false;
            }
            continue;
        case 5:
            if (b->animation_frame >= 32) {
                b->scroll_line = 400;
                b->entrance_stage = 7;
                continue;
            }
            b->entrance_stage = 6;
            return M_Scroll(b, vsync, 416 - b->animation_frame % 2 * 32);
        case 6:
            if (b->animation_frame % 8 == 0) {
                M_Effect(c, 9);
            }
            b->animation_frame++;
            b->entrance_stage = 5;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 7:
            if (b->scroll_line < 0) {
                b->animation_frame = 0;
                b->entrance_stage = 9;
                continue;
            }
            b->entrance_stage = 8;
            return M_Scroll(b, vsync, b->scroll_line);
        case 8:
            for (int32_t i = 0; i < 32 * 80; i++) {
                const uint16_t offset =
                    (uint16_t)(b->scroll_line * 80 + i) & 0x7fff;
                if (offset < 32000) {
                    memcpy(
                        M_Video(c)->pages[0] + offset * 8,
                        M_Video(c)->pages[1] + offset * 8, 8);
                }
            }
            Video_SelectDrawingPage(M_Video(c), 0);
            b->scroll_line -= 32;
            b->entrance_stage = 7;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 9:
            if (b->animation_frame >= 32) {
                b->entrance_stage = 11;
                continue;
            }
            b->entrance_stage = 10;
            return M_Scroll(b, vsync, 416 - b->animation_frame % 2 * 16);
        case 10:
            b->animation_frame++;
            b->entrance_stage = 9;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 11:
            b->entrance_stage = 12;
            if (!M_Delay(b, vsync, 30)) {
                return false;
            }
            continue;
        case 12:
            M_White(c);
            for (int32_t i = 0; i < 4; i++) {
                M_Pi(c, &c->assets->layers[i], true);
            }
            b->animation_frame = 0;
            b->entrance_stage = 13;
            continue;
        case 13:
            if (b->animation_frame >= 16) {
                VIDEO *const video = M_Video(c);
                Video_Copy(
                    video, video->drawing_page, (GFX_RECT) { 0, 0, 640, 400 },
                    1 - video->drawing_page, 0, 0);
                b->entrance_active = false;
                return true;
            }
            M_WhiteIn(c);
            b->animation_frame++;
            if (!M_Delay(b, vsync, 10)) {
                return false;
            }
            continue;
        default:
            ASSERT(false);
            return false;
        }
    }
}

static void M_Single(
    const B20J_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        PELLET_REGULAR, 0, 0, 0);
}
static void M_Rain(
    const B20J_CONTEXT *const c, const int16_t x, const int16_t y,
    const uint8_t angle, const int16_t speed)
{
    Pellets_AddSingle(
        c->pellets, c->pellet_shared, c->hit->display->rank, x, y, angle, speed,
        PELLET_GRAVITY, 1, 0, 0);
}
static void M_Group(
    const B20J_CONTEXT *const c, const int16_t x, const int16_t y,
    const int16_t group, const int16_t speed)
{
    Pellets_AddGroup(
        c->pellets, c->pellet_shared, M_Player(c), c->rng,
        c->hit->display->rank, x, y, group, speed);
}
static uint8_t M_Angle(const int32_t y, const int32_t x)
{
    return Math_Angle(M_W(y), M_W(x));
}
static void M_Diamond(
    const B20J_CONTEXT *const c, const int16_t left, const int16_t top,
    const uint8_t color)
{
    if (left < 0 || left >= 640 || top < 0 || top >= 400) {
        return;
    }
    for (int32_t y = 0; y < 8; y++) {
        for (int32_t x = 0; x < 8; x++) {
            const int32_t offset = (top + y) * 640 + left + x;
            if (offset < VIDEO_PAGE_SIZE
                && (c->assets->diamond[y] & (0x80u >> x)) != 0) {
                M_Video(c)->pages[M_Video(c)->drawing_page][offset] = color;
            }
        }
    }
}
static void M_Erase(
    const B20J_CONTEXT *const c, const int16_t left, const int16_t top)
{
    Sprite_Restore16(M_Video(c), left, top, 16, 8);
}
static void M_Cross(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 0);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->pattern_state = M_Rank(
            c, PELLET_GROUP_TWO_NARROW_AIMED, PELLET_GROUP_THREE_NARROW_AIMED,
            PELLET_GROUP_FIVE_WIDE_AIMED, PELLET_GROUP_FIVE_NARROW_AIMED);
        b->diamond_velocity[0] = Math_VectorBetween(
            (MATH_VECTOR) { 316, 236 }, (MATH_VECTOR) { 0, 384 }, 7);
        b->diamond_velocity[1] = Math_VectorBetween(
            (MATH_VECTOR) { 316, 236 }, (MATH_VECTOR) { 0, 64 }, 7);
        for (int32_t i = 0; i < 4; i++) {
            b->diamond_left[i] = 316;
            b->diamond_top[i] = 236;
        }
        M_Group(c, 312, 188, b->pattern_state, 48);
        b->pattern_state = M_Rank(c, 18, 16, 14, 12);
        M_Effect(c, 12);
    } else if (b->diamond_left[0] > 0) {
        for (int32_t i = 0; i < 4; i++) {
            M_Erase(c, b->diamond_left[i], b->diamond_top[i]);
        }
        for (int32_t i = 0; i < 4; i++) {
            b->diamond_left[i] =
                M_W(b->diamond_left[i]
                    + (i % 2 == 0 ? 1 : -1) * b->diamond_velocity[i / 2].x);
            b->diamond_top[i] =
                M_W(b->diamond_top[i] + b->diamond_velocity[i / 2].y);
        }
        if (b->diamond_left[0] <= 0) {
            b->diamond_left[0] = 0;
            b->diamond_left[2] = 0;
            b->diamond_left[1] = 632;
            b->diamond_left[3] = 632;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Diamond(c, b->diamond_left[i], b->diamond_top[i], 9);
            }
        }
        return;
    } else if (b->diamond_top[0] > 64) {
        for (int32_t i = 0; i < 4; i++) {
            M_Erase(c, b->diamond_left[i], b->diamond_top[i]);
        }
        b->diamond_top[0] = M_W(b->diamond_top[0] - 3);
        b->diamond_top[1] = M_W(b->diamond_top[1] - 3);
        b->diamond_left[2] = M_W(b->diamond_left[2] + 6);
        b->diamond_left[3] = M_W(b->diamond_left[3] - 6);
        if (b->diamond_top[0] <= 64) {
            b->diamond_top[0] = 64;
        } else {
            for (int32_t i = 0; i < 4; i++) {
                M_Diamond(c, b->diamond_left[i], b->diamond_top[i], 9);
            }
        }
        return;
    } else if (b->diamond_edges_frame < 200) {
        b->diamond_edges_frame++;
        if (b->diamond_edges_frame % b->pattern_state == 0) {
            int16_t from_y = 64 + M_Random(c) % 200;
            int16_t to_x = 240 + M_Random(c) % 400;
            M_Single(c, 0, from_y, M_Angle(400 - from_y, to_x), 40);
            from_y = 64 + M_Random(c) % 200;
            to_x = M_Random(c) % 400;
            M_Single(c, 632, from_y, M_Angle(400 - from_y, to_x - 632), 40);
            for (int32_t i = 0; i < 2; i++) {
                const int16_t from_x = M_Random(c) % 640;
                to_x = M_Random(c) % 640;
                M_Single(c, from_x, 64, M_Angle(336, to_x - from_x), 40);
            }
            const int16_t from_x = M_Random(c) % 640;
            M_Group(c, from_x, 64, PELLET_GROUP_SINGLE_AIMED, 40);
        }
        return;
    } else {
        b->frame = 0;
    }
    b->diamond_edges_frame = 0;
}
static void M_CupFire(
    B20J *const b, const B20J_CONTEXT *const c, const uint8_t angle)
{
    M_Single(c, 328, 188, angle, b->pattern_state);
    M_Single(c, 296, 188, angle, b->pattern_state);
}
static void M_Cup(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 1);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->cup_angle = 64;
        b->pattern_state = M_Rank(c, 80, 80, 96, 112);
    }
    if (b->frame < 140 && b->frame % 8 == 0) {
        M_CupFire(b, c, 64);
        return;
    }
    if (b->frame < 220 && b->frame % 8 == 0) {
        b->cup_angle += 5;
        M_CupFire(b, c, b->cup_angle);
        M_CupFire(b, c, 128 - b->cup_angle);
        return;
    }
    if (b->frame < 300 && b->frame % 8 == 0) {
        b->cup_angle -= 12;
        M_CupFire(b, c, b->cup_angle);
        M_CupFire(b, c, 128 - b->cup_angle);
    }
    if (b->frame >= 300) {
        b->frame = 0;
    }
}
static void M_SnakeAim(
    B20J_SNAKES *const s, const B20J_CONTEXT *const c, const int32_t i,
    const int16_t target, const int16_t speed)
{
    uint8_t angle = M_Angle(384 - s->top[i][0], target + 12 - s->left[i][0]);
    angle += M_Random(c) % 2 == 0 ? 40 : -40;
    s->velocity[i] = Math_Vector(speed, angle);
}
static void M_SnakeSpawn(
    B20J_SNAKES *const s, const B20J_CONTEXT *const c, const int32_t i,
    const int16_t x, const int16_t y)
{
    for (int32_t j = 0; j < 5; j++) {
        s->left[i][j] = x;
        s->top[i][j] = y;
    }
    M_SnakeAim(s, c, i, M_Player(c)->left, 6);
}
static void M_SnakesUpdate(
    B20J_SNAKES *const s, const B20J_CONTEXT *const c, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (s->left[i][0] == 999) {
            continue;
        }
        M_Erase(c, s->left[i][4], s->top[i][4]);
        for (int32_t j = 3; j >= 1; j--) {
            M_Diamond(c, s->left[i][j], s->top[i][j], 9);
        }
        M_Diamond(c, s->left[i][0], s->top[i][0], 7);
        if (!s->target_locked[i]) {
            s->target_left[i] = M_Player(c)->left;
        }
        M_SnakeAim(s, c, i, s->target_left[i], 7);
        if (s->top[i][0] > 300) {
            s->target_locked[i] = true;
        }
        for (int32_t j = 4; j >= 1; j--) {
            s->left[i][j] = s->left[i][j - 1];
            s->top[i][j] = s->top[i][j - 1];
        }
        s->left[i][0] = M_W(s->left[i][0] + s->velocity[i].x);
        s->top[i][0] = M_W(s->top[i][0] + s->velocity[i].y);
        PLAYER *const player = M_Player(c);
        if (s->left[i][0] > player->left && s->left[i][0] < player->left + 24
            && s->top[i][0] > 368 + (player->sliding ? 10 : 0)
            && s->top[i][0] < 392 && !player->invincible) {
            player->is_hit = true;
        }
    }
}
static void M_Snakes(
    B20J *const b, const B20J_CONTEXT *const c, const bool four)
{
    B20J_SNAKES *const s = four ? &b->four_snakes : &b->two_snakes;
    const int32_t count = four ? 4 : 2;
    if (b->frame == 10) {
        M_Expression(b, c, 2);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        for (int32_t i = 0; i < count; i++) {
            s->target_locked[i] = false;
        }
        M_SnakeSpawn(s, c, 0, 312, 188);
        for (int32_t i = 1; i < count; i++) {
            s->left[i][0] = 999;
        }
        if (four) {
            b->pattern_state = M_Rank(c, 18, 16, 14, 12);
        }
        M_Effect(c, 12);
        return;
    }
    if (b->frame < (four ? 400 : 500)) {
        M_SnakesUpdate(s, c, count);
        if (b->frame == 150) {
            M_SnakeSpawn(s, c, 1, 312, 188);
            M_Effect(c, 12);
        }
        if (four && (b->frame == 200 || b->frame == 250)) {
            M_SnakeSpawn(s, c, b->frame == 200 ? 2 : 3, 290, 200);
            M_Effect(c, 12);
        }
        if (!four && b->frame > 240 - (int32_t)c->hit->display->rank * 40
            && b->frame % 40 == 0) {
            uint8_t angle = M_Random(c) % 12;
            const int16_t x = b->frame % 120 == 0 ? 410
                : b->frame % 120 == 40            ? 316
                                                  : 312;
            const int16_t y = b->frame % 120 == 0 ? 70
                : b->frame % 120 == 40            ? 140
                                                  : 188;
            for (int32_t i = 0; i < 10; i++) {
                const int16_t speed = 40
                    + (i % 4 == 1       ? 16
                           : i % 4 == 3 ? 32
                                        : 0);
                M_Single(c, x, y, 128 - angle, speed);
                M_Single(c, x, y, angle, speed);
                angle += 12;
            }
            M_Effect(c, 7);
        }
    } else {
        for (int32_t i = 0; i < count; i++) {
            for (int32_t j = 0; j < 5; j++) {
                M_Erase(c, s->left[i][j], s->top[i][j]);
            }
        }
        b->frame = 0;
    }
}
static void M_Rows(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 0);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->row_velocity = Math_VectorBetween(
            (MATH_VECTOR) { 290, 200 }, (MATH_VECTOR) { 64, 64 }, 16);
        b->row_left = 290;
        b->row_top = 200;
        b->row_speed = 48;
        b->row_direction = 99;
        M_Effect(c, 12);
        b->pattern_state = M_Rank(c, 12, 10, 8, 6);
        return;
    }
    if (b->row_direction == 99) {
        M_Erase(c, b->row_left, b->row_top);
        b->row_left = M_W(b->row_left + b->row_velocity.x);
        b->row_top = M_W(b->row_top + b->row_velocity.y);
        if (b->row_top <= 64) {
            b->row_left = 64;
            b->row_top = 64;
            b->row_direction = 0;
            return;
        }
        M_Diamond(c, b->row_left, b->row_top, 9);
    } else if (b->row_direction < 99) {
        M_Erase(c, b->row_left, b->row_top);
        b->row_left =
            M_W(b->row_left
                + (b->row_direction == 0       ? 8
                       : b->row_direction == 1 ? -8
                                               : 0));
        if (b->row_direction >= 2) {
            b->row_top = M_W(b->row_top + 8);
            b->row_direction++;
            if (b->row_direction >= 10) {
                b->row_direction = b->row_left > 320 ? 1 : 0;
            }
        }
        if (b->row_left > 576) {
            b->row_direction = 2;
            b->row_left = 576;
        } else if (b->row_left < 64) {
            b->row_direction = 2;
            b->row_left = 64;
        }
        if (b->row_top >= 256) {
            b->frame = 0;
            return;
        }
        if (b->row_direction < 2 && b->frame % b->pattern_state == 0) {
            M_Group(
                c, b->row_left, b->row_top, PELLET_GROUP_SINGLE_AIMED,
                b->row_speed);
            b->row_speed = M_W(b->row_speed + 2);
        }
        M_Diamond(c, b->row_left, b->row_top, 6);
    }
}
static void M_Spray(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 1);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->spray_offset = 32;
        b->spray_delta = -8;
        b->spray_fired = 0;
        b->pattern_state = M_Rank(c, 5, 4, 3, 2);
    }
    if (b->frame % b->pattern_state == 0) {
        b->spray_angle =
            M_Angle(368 - 184, M_Player(c)->left - 278) + b->spray_offset;
        b->spray_offset += b->spray_delta;
        b->spray_fired++;
        if (b->spray_fired > 8) {
            b->spray_delta *= -1;
            b->spray_fired = 0;
        }
        M_Single(c, 312, 188, b->spray_angle, 48);
    }
    if (b->frame >= 700) {
        b->frame = 0;
    }
}
static void M_Ray(
    B20J *const b, const B20J_CONTEXT *const c, const int16_t dx,
    const int16_t dy)
{
    if (b->ray_previous_x >= 0 && b->ray_previous_x < 640
        && b->ray_previous_y >= 0 && b->ray_previous_y < 400) {
        Video_LineErase(
            M_Video(c), 410, 70, b->ray_previous_x, b->ray_previous_y);
    }
    if (b->ray_x >= 0 && b->ray_x < 640 && b->ray_y >= 0 && b->ray_y < 400) {
        Video_Line(M_Video(c), 410, 70, b->ray_x, b->ray_y, 6);
    }
    b->ray_previous_x = b->ray_x;
    b->ray_previous_y = b->ray_y;
    b->ray_x = M_W(b->ray_x + dx);
    b->ray_y = M_W(b->ray_y + dy);
}
static void M_Edges(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 3);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->ray_x = 0;
        b->ray_y = 264;
        b->pattern_state = M_Rank(c, 5, 3, 2, 2);
    }
    if (b->frame < 125) {
        M_Ray(b, c, 0, -8);
    } else if (b->frame <= 125) {
        b->ray_x = 0;
        b->ray_y = 64;
        M_Ray(b, c, 8, 0);
    } else if (b->frame < 205) {
        M_Ray(b, c, 8, 0);
    } else if (b->frame == 205) {
        b->ray_x = 639;
        b->ray_y = 64;
        M_Ray(b, c, 0, 8);
    } else if (b->frame < 230) {
        M_Ray(b, c, 0, 8);
    } else if (b->frame == 230) {
        b->ray_x = 639;
        b->ray_y = 264;
        M_Ray(b, c, 0, -8);
    } else if (b->frame < 255) {
        M_Ray(b, c, 0, -8);
    } else if (b->frame == 255) {
        b->ray_x = 639;
        b->ray_y = 64;
        M_Ray(b, c, -8, 0);
    } else if (b->frame < 335) {
        M_Ray(b, c, -8, 0);
    } else if (b->frame == 335) {
        b->ray_x = 0;
        b->ray_y = 64;
        M_Ray(b, c, 0, 8);
    } else if (b->frame < 360) {
        M_Ray(b, c, 0, 8);
    } else if (b->frame == 360) {
        b->ray_y = M_W(b->ray_y - 8);
        Video_LineErase(M_Video(c), 410, 70, b->ray_x, b->ray_y);
        b->frame = 0;
    }
    if (b->frame % 10 == 0) {
        M_Effect(c, 6);
    }
    if (b->frame % b->pattern_state == 0) {
        M_Rain(c, b->ray_x, b->ray_y, M_Random(c) & 127, 32);
        M_Rain(c, b->ray_x, b->ray_y, M_Random(c) & 127, 32);
    }
}
static void M_SlashAnimate(B20J *const b, const B20J_CONTEXT *const c)
{
    const int16_t frames[] = { 50, 60, 100, 120, 140, 160, 170 };
    for (int32_t i = 0; i < 7; i++) {
        if (b->frame < frames[i]) {
            return;
        }
        if (b->frame == frames[i]) {
            M_SlashPut(c, i);
            if (i == 0) {
                M_Effect(c, 8);
            }
        }
    }
    if (b->frame > 200) {
        b->frame = 0;
        b->direction_can_change = true;
    }
}
static void M_SlashFire(
    B20J *const b, const B20J_CONTEXT *const c, const int32_t pattern)
{
    const int16_t x = b->slash_left[pattern], y = b->slash_top[pattern];
    if (pattern == 0) {
        M_Rain(c, x, y, M_Random(c) % 127, 0);
        M_Rain(c, x, y, M_Random(c) % 127, 0);
    } else if (pattern == 1) {
        M_Single(c, x, y, 32, b->pattern_state);
        M_Single(c, x, y, 96, b->pattern_state);
    } else {
        M_Group(c, x, y, PELLET_GROUP_SINGLE_AIMED, b->pattern_state);
    }
}
static void M_Slash(
    B20J *const b, const B20J_CONTEXT *const c, const int32_t pattern)
{
    if (b->frame == 10) {
        M_Direction(b, c, 2);
        M_Expression(b, c, pattern == 0 ? 1 : 3);
        b->direction_can_change = false;
        b->slash_left[pattern] = 410;
        b->slash_top[pattern] = 70;
        b->pattern_state = pattern == 0 ? M_Rank(c, 5, 3, 2, 1)
            : pattern == 1              ? M_Rank(c, 32, 48, 64, 72)
                                        : M_Rank(c, 64, 80, 88, 96);
    }
    M_SlashAnimate(b, c);
    if (b->frame < 100) {
        return;
    }
    const int16_t interval = pattern == 0 ? b->pattern_state : 3;
    if (b->frame < 140 && b->frame % interval == 0) {
        M_SlashFire(b, c, pattern);
        b->slash_left[pattern] =
            M_W(b->slash_left[pattern] + 22 / (40 / interval));
        b->slash_top[pattern] =
            M_W(b->slash_top[pattern] + 162 / (40 / interval));
    }
    if (b->frame == 140) {
        b->slash_left[pattern] = 432;
        b->slash_top[pattern] = 232;
        if (pattern == 0) {
            b->pattern_state = M_Rank(c, 3, 2, 2, 2);
        }
    }
    if (b->frame < 140) {
        return;
    }
    if (b->frame < 150) {
        M_SlashFire(b, c, pattern);
        b->slash_left[pattern] = M_W(b->slash_left[pattern] - (-234 / -10));
        b->slash_top[pattern] = M_W(b->slash_top[pattern] - (-34 / -10));
    }
    if (b->frame == 150) {
        b->slash_left[pattern] = 198;
        b->slash_top[pattern] = 198;
        if (pattern == 0) {
            b->pattern_state = M_Rank(c, 3, 2, 1, 1);
        }
    }
    if (b->frame < 150) {
        return;
    }
    const int16_t last_interval = pattern == 0 ? b->pattern_state : 2;
    if (b->frame < 170 && b->frame % last_interval == 0) {
        M_SlashFire(b, c, pattern);
        b->slash_left[pattern] =
            M_W(b->slash_left[pattern] - (-212 / (20 / last_interval)));
        b->slash_top[pattern] =
            M_W(b->slash_top[pattern] - (128 / (20 / last_interval)));
    }
}
static void M_Lasers(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 3);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame == 100) {
        b->laser_right_to_left = M_Random(c) % 2 != 0;
        b->pattern_state = M_Rank(c, 128, 96, 80, 64);
    }
    if (b->frame % 10 == 0) {
        const int16_t distance = M_W((b->frame - 100) / 10 * b->pattern_state);
        const int16_t target =
            M_W(b->laser_right_to_left ? 640 - distance : distance);
        ShootoutLasers_Spawn(
            &c->lasers->slots[(b->frame / 10) % 10], 410, 70, target, 400, 68,
            7, 30, 5);
        M_Effect(c, 6);
        if ((!b->laser_right_to_left && target >= 640)
            || (b->laser_right_to_left && target <= 0)) {
            b->frame = 0;
        }
        M_Group(c, 410, 70, PELLET_GROUP_THREE_WIDE_AIMED, 72);
    }
}
static void M_Sleeve(B20J *const b, const B20J_CONTEXT *const c)
{
    if (b->frame == 10) {
        M_Expression(b, c, 3);
    }
    if (b->frame < 100) {
        return;
    }
    if (b->frame % 20 == 0 || b->frame % 20 == 10) {
        uint8_t angle = 0;
        for (int32_t i = 0; i < 10; i++, angle -= 12) {
            if (b->frame % 20 == 0) {
                M_Rain(c, 290, 200, angle, 32);
            } else {
                Pellets_AddSingle(
                    c->pellets, c->pellet_shared, c->hit->display->rank, 290,
                    200, angle, 32, PELLET_FALL_STRAIGHT_FROM_TOP_THEN_REGULAR,
                    48, 0, 0);
            }
        }
        M_Effect(c, 7);
    }
    if (b->frame >= 300) {
        b->frame = 0;
    }
}

static void M_BeginDeath(B20J *const b, const B20J_CONTEXT *const c)
{
    Tram_Clear(c->tram);
    Video_Clear(M_Video(c), 0);
    M_Audio(c, B20J_AUDIO_STOP);
    b->defeating = true;
    b->defeat_stage = 1;
    b->animation_frame = 7;
}
static bool M_Death(
    B20J *const b, const B20J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (!M_Ready(b, c, vsync)) {
        return false;
    }
    for (;;) {
        switch (b->defeat_stage) {
        case 1:
            if (b->animation_frame > 15) {
                b->frame = 0;
                b->defeat_stage = 2;
                continue;
            }
            if (b->animation_frame != 7) {
                Video_Clear(M_Video(c), 0);
            }
            const int16_t flash = b->animation_frame % 2 == 0 ? 15 : 0;
            Palette_SetColor(
                c->hit->palette, M_Video(c), 0, flash, flash, flash);
            M_Grx(c, b->animation_frame);
            b->animation_frame++;
            if (!M_Delay(b, vsync, 10)) {
                return false;
            }
            continue;
        case 2:
            b->frame = M_W(b->frame + 1);
            if (b->frame % 4 == 0) {
                M_BlackOut(c);
                M_Effect(c, 7);
            }
            b->defeat_stage = 3;
            return M_Scroll(b, vsync, 408 + b->frame % 2 * 16);
        case 3:
            if (b->frame > 64) {
                Tram_Fill(c->tram, (GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLACK);
                M_Pi(c, &c->assets->scroll, false);
                Palette_SetGray(c->hit->palette, M_Video(c), 0);
                Tram_Clear(c->tram);
                b->scroll_line = 0;
                b->scroll_frame = 0;
                b->defeat_stage = 4;
                continue;
            }
            b->defeat_stage = 2;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 4:
            b->defeat_stage = 5;
            return M_Scroll(b, vsync, b->scroll_line);
        case 5:
            b->scroll_line += 32;
            if (b->scroll_line > 400) {
                b->scroll_line -= 400;
            }
            if (b->scroll_frame < 150) {
                if (b->scroll_frame % 8 == 0) {
                    Palette_StepTo(
                        c->hit->palette, M_Video(c), &c->hit->palette->resource,
                        true);
                    M_Effect(c, 7);
                }
            } else if (b->scroll_frame % 8 == 0) {
                if (b->scroll_frame == 192) {
                    M_Audio(c, B20J_AUDIO_FADE);
                }
                M_BlackOut(c);
                if (c->hit->palette->saved.colors[7][0] == 0) {
                    b->defeat_stage = 6;
                    return M_Scroll(b, vsync, 0);
                }
                if (b->scroll_frame < 220) {
                    M_Effect(c, 7);
                }
            }
            b->scroll_frame++;
            b->defeat_stage = 4;
            if (!M_Delay(b, vsync, 1)) {
                return false;
            }
            continue;
        case 6:
            for (int32_t i = 0; i < 5; i++) {
                c->hit->display->gameplay->score += 10000;
            }
            *c->game_cleared = true;
            b->finished = true;
            b->defeating = false;
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}
static void M_Hit(B20J *const b, const B20J_CONTEXT *const c)
{
    BOSS_HIT_CONTEXT hit = *c->hit;
    hit.hp = &b->hp;
    hit.boss_palette = &b->boss_palette;
    const uint8_t colors[] = { 3, 4, 5 };
    const ORB *const orb = hit.orb;
    const bool collision = orb->left >= 288 && orb->left <= 352
        && orb->top >= 120 && orb->top <= 160;
    BossHit_UpdateAndRender(
        &b->hit, &hit, colors, 3, 10000, collision,
        (BOSS_SHOT_HITBOX) { 288, 120, 96, 40 });
}
static void M_Next(
    B20J *const b, const B20J_CONTEXT *const c, const int8_t phase)
{
    b->phase = phase;
    b->frame = 0;
    b->hit.frame = 0;
    b->pattern = 99;
    b->patterns_done = 0;
    if ((phase & 1) != 0) {
        M_Expression(b, c, 0);
    }
}
static void M_Pattern(
    B20J *const b, const B20J_CONTEXT *const c, const int16_t pattern)
{
    switch (pattern) {
    case 0:
        M_Cross(b, c);
        break;
    case 1:
        M_Cup(b, c);
        break;
    case 2:
        M_Snakes(b, c, false);
        break;
    case 3:
        M_Rows(b, c);
        break;
    case 4:
        M_Spray(b, c);
        break;
    case 5:
        M_Snakes(b, c, true);
        break;
    case 6:
        M_Edges(b, c);
        break;
    case 7:
        M_Slash(b, c, 0);
        break;
    case 8:
        M_Lasers(b, c);
        break;
    case 9:
        M_Slash(b, c, 1);
        break;
    case 10:
        M_Sleeve(b, c);
        break;
    case 11:
        M_Slash(b, c, 2);
        break;
    default:
        break;
    }
}
bool B20J_UpdateAndRender(
    B20J *const b, const B20J_CONTEXT *const c, FRAME_VSYNC *const vsync)
{
    if (b->finished) {
        return true;
    }
    if (b->entrance_active) {
        return B20J_ResumeEntrance(b, c, vsync);
    }
    if (b->defeating) {
        return M_Death(b, c, vsync);
    }
    if (b->phase == 0) {
        b->phase = 1;
        b->pattern_previous = 99;
        b->pattern = 99;
        b->patterns_done = 0;
        b->frame = 0;
        b->hit.frame = 0;
        b->hit.invincible = false;
        b->hp_bar_filled = false;
        b->boss_palette = c->hit->palette->saved;
        *c->rng = c->hit->frame_rand;
        return true;
    }
    if (b->phase < 1 || b->phase > 7) {
        return true;
    }
    if (b->phase == 1 && !b->hp_bar_filled) {
        b->hp_bar_filled =
            HudHp_Increment(c->hit->health, M_Video(c), b->hp, b->frame);
    }
    b->frame = M_W(b->frame + 1);
    b->hit.frame = M_W(b->hit.frame + 1);
    const int16_t direction = M_Player(c)->left < 198 ? 1
        : M_Player(c)->left > 396                     ? 0
                                                      : 2;
    M_Direction(b, c, direction);
    if ((b->phase & 1) == 0) {
        if (b->frame == 50) {
            M_White(c);
        }
        if (b->frame > 50 && b->frame % 4 == 0) {
            M_WhiteIn(c);
        }
        M_Hit(b, c);
        if (!b->hit.invincible && b->frame > 120) {
            M_Next(b, c, b->phase + 1);
        }
        return true;
    }
    if (b->pattern == 99) {
        if (b->frame > (b->phase == 7 ? 5 : 120)) {
            b->frame = 1;
            const int16_t count = b->phase == 1 ? 3
                : b->phase == 3                 ? 4
                : b->phase == 5                 ? 3
                                                : 12;
            const int16_t retry = b->phase >= 5 ? 2 : count;
            b->pattern = M_Random(c) % count;
            if (b->pattern == b->pattern_previous) {
                b->pattern = M_Random(c) % retry;
            }
            b->pattern_previous = b->pattern;
            b->patterns_done = M_W(b->patterns_done + 1);
        }
    } else {
        const int16_t pattern = b->phase == 3 ? b->pattern + 3
            : b->phase == 5                   ? b->pattern + 7
                                              : b->pattern;
        M_Pattern(b, c, pattern);
    }
    if (b->frame == 0) {
        M_Expression(b, c, 0);
        b->pattern = 99;
    }
    M_Hit(b, c);
    if (b->phase == 7) {
        if (b->hp <= 0) {
            M_BeginDeath(b, c);
            return M_Death(b, c, vsync);
        }
    } else if (!b->hit.invincible && b->pattern == 99) {
        const int16_t patterns = b->phase == 1 ? 7 : b->phase == 3 ? 9 : 6;
        const int16_t threshold = b->phase == 1 ? 16 : b->phase == 3 ? 13 : 10;
        if (b->patterns_done >= patterns || b->hp < threshold) {
            M_Next(b, c, b->phase + 1);
        }
    }
    return true;
}

RESULT B20J_ValidateContext(const B20J_CONTEXT *const c)
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
            || c->hit->display->video == nullptr
            || c->hit->display->player == nullptr
            || c->hit->display->gameplay == nullptr || c->hit->shots == nullptr
            || c->hit->orb == nullptr || c->hit->test_damage == nullptr
            || c->hit->effect == nullptr || c->pellets == nullptr
            || c->pellet_shared == nullptr || c->lasers == nullptr
            || c->rng == nullptr || c->objects == nullptr || c->tram == nullptr
            || c->game_cleared == nullptr || c->audio == nullptr,
        "Missing Konngara context");
    MUST(B20J_ValidateAssets(c->assets));
    FAIL_IF(
        c->hit->health->backgrounds == nullptr
            || c->hit->hud->numbers == nullptr || c->tram->pixels == nullptr,
        "Missing Konngara display storage");
    MUST(Shots_ValidateSprites(c->hit->display->sprites));
    for (uint32_t digit = 0xff10; digit <= 0xff19; digit++) {
        FAIL_IF(
            GameText_Find(c->hit->display->font, digit) == nullptr,
            "Missing Konngara score glyph");
    }
    return OK;
}
