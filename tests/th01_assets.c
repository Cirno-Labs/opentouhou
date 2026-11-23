#include "th01_assets.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    GAMEPLAY_ASSETS assets;
    STAGE_DATA_STAGE layouts[5];
    PELLET_ASSETS pellets;
    SHOOTOUT_LASERS_ASSETS lasers;
    MDRV_TRACK track;
    MDRV_EFFECT effects[17];
    uint8_t music_bytes[3], effect_bytes[118];
} M_ASSETS;

static void M_BossAssets(M_ASSETS *const f, const ALLOCATOR *const a)
{
    GAMEPLAY_ASSETS *const assets = &f->assets;
    uint8_t *const pixels = Allocator_AllocZ(a, VIDEO_PAGE_SIZE);
    uint8_t *const coverage = Allocator_AllocZ(a, VIDEO_PAGE_SIZE);
    const PI_IMAGE image = { .width = VIDEO_WIDTH,
                             .height = VIDEO_HEIGHT,
                             .has_palette = true,
                             .pixels = pixels };
    assets->b05.sphere = (BOS) { 96, 96, 8, pixels, coverage };
    assets->b05.flash = (BOS) { 96, 96, 3, pixels, coverage };
    assets->b05.person = (BOS) { 96, 96, 5, pixels, coverage };
    assets->b05.background = image;
    assets->b10j.still = (BOS) { 128, 160, 1, pixels, coverage };
    assets->b10j.animation = (BOS) { 128, 64, 5, pixels, coverage };
    assets->b10j.background = image;
    assets->b10j.missiles = assets->stage;
    assets->b10j.next_scene = assets->scenes[4];
    assets->b10m.eyes = (BOS) { 64, 48, 7, pixels, coverage };
    assets->b10m.background = image;
    assets->b10m.missiles = assets->stage;
    assets->b10m.next_scene = assets->scenes[3];
    assets->b15j.souls = (BOS) { 32, 32, 3, pixels, coverage };
    assets->b15j.tears = (BOS) { 16, 16, 3, pixels, coverage };
    assets->b15j.ripples = (PTN) { .count = 1, .pixels = pixels };
    assets->b15j.background = image;
    assets->b15j.next_scene = assets->scenes[6];
    assets->b15m.girl = (BOS) { 128, 96, 6, pixels, coverage };
    assets->b15m.attack = (BOS) { 128, 96, 3, pixels, coverage };
    assets->b15m.bat = (BOS) { 48, 32, 3, pixels, coverage };
    assets->b15m.spheres = (GRC) { 32, 32, 6, coverage };
    assets->b15m.star = pixels;
    assets->b15m.background = image;
    assets->b15m.missiles = assets->stage;
    assets->b15m.next_scene = assets->scenes[5];
    assets->b20j.head = (BOS) { 80, 80, 3, pixels, coverage };
    assets->b20j.face = (BOS) { 80, 32, 6, pixels, coverage };
    assets->b20j.aim = (BOS) { 80, 32, 3, pixels, coverage };
    assets->b20j.body.count = 16;
    for (int32_t i = 0; i < 16; i++) {
        assets->b20j.body.images[i] =
            (GRX) { .stream_count = 1, .pixels = pixels, .coverage = coverage };
    }
    assets->b20j.scroll = image;
    assets->b20j.upper = image;
    for (int32_t i = 0; i < 4; i++) {
        assets->b20j.layers[i] = image;
    }
    assets->b20j.diamond = pixels;
    assets->b20j.flake = pixels;
    assets->b20m.shield = (BOS) { 48, 48, 4, pixels, coverage };
    assets->b20m.wand = (BOS) { 96, 96, 2, pixels, coverage };
    assets->b20m.dress = (BOS) { 96, 96, 4, pixels, coverage };
    assets->b20m.vortex = (GRC) { 32, 32, 6, coverage };
    assets->b20m.bird = (GRC) { 32, 32, 8, coverage };
    assets->b20m.cross = (GRC) { 32, 32, 2, coverage };
    assets->b20m.splash = (GRC) { 32, 32, 3, coverage };
    assets->b20m.lower = image;
    assets->b20m.upper = image;
    for (int32_t i = 0; i < 6; i++) {
        assets->b20m.backgrounds[i] = image;
    }
    assets->b20m.star = pixels;
    assets->b20m.flake = pixels;
    assets->b20m.spark = pixels;
    assets->b20m.leaf_left = pixels;
    assets->b20m.leaf_right = pixels;
    assets->b20m.interleave = Allocator_AllocZ(a, sizeof(INTERLEAVE_ASSETS));
}

GAMEPLAY_ASSETS *Th01TestAssets_Create(const ALLOCATOR *const a)
{
    M_ASSETS *const f = Allocator_AllocZ(a, sizeof(*f));
    f->assets.stage =
        (PTN) { .count = 34, .pixels = Allocator_Alloc(a, 34 * 1024) };
    memset(f->assets.stage.pixels, PTN_TRANSPARENT, 34 * 1024);
    f->assets.boss_stage = f->assets.stage;
    f->assets.miko = f->assets.stage;
    f->assets.hud.coverage = Allocator_AllocZ(a, 640 * 66);
    f->assets.pellets = &f->pellets;
    f->assets.lasers = &f->lasers;
    GAME_TEXT_GLYPH glyphs[347] = {};
    for (uint32_t i = 0; i < 95; i++) {
        glyphs[i] = (GAME_TEXT_GLYPH) { .codepoint = i + 32, .width = 8 };
    }
    for (uint32_t i = 0; i < 240; i++) {
        glyphs[95 + i] = (GAME_TEXT_GLYPH) {
            .codepoint = 0xff00 + i,
            .width = i >= 0x61 && i <= 0x9f ? 8 : 16,
        };
    }
    const uint32_t title[] = { 0x6771, 0x65b9, 0x2605, 0x9748, 0x7570, 0x4f1d };
    for (int32_t i = 0; i < 6; i++) {
        glyphs[335 + i] =
            (GAME_TEXT_GLYPH) { .codepoint = title[i], .width = 16 };
    }
    glyphs[341] = (GAME_TEXT_GLYPH) { .codepoint = 0x3000, .width = 16 };
    const uint32_t route[] = { 0x9b54, 0x754c, 0x3078, 0x5730, 0x7344 };
    for (int32_t i = 0; i < 5; i++) {
        glyphs[342 + i] =
            (GAME_TEXT_GLYPH) { .codepoint = route[i], .width = 16 };
    }
    ASSERT(Result_Ignore(
        GameText_LoadGlyphs(glyphs, 347, a, &f->assets.hud_font)));
    f->assets.menu_font = f->assets.hud_font;
    f->assets.item_font = f->assets.hud_font;
    f->assets.bonus_font = f->assets.hud_font;
    f->assets.route_font = f->assets.hud_font;
    for (int32_t i = 0; i < 7; i++) {
        f->assets.scenes[i].id = i;
        f->assets.scenes[i].layouts.stages = f->layouts;
        f->assets.backgrounds[i].pixels = Allocator_AllocZ(a, VIDEO_PAGE_SIZE);
    }
    for (int32_t i = 0; i < 5; i++) {
        f->layouts[i].cells[0][0].card = true;
    }
    const uint8_t effect[] = { 0xeb, 0x80, 0xec, 15, 0x48, 8, 0xff };
    memcpy(f->effect_bytes + 88, effect, sizeof(effect));
    const uint8_t envelope[] = { 128, 64, 16, 96, 3, 7 };
    memcpy(f->effect_bytes + 112, envelope, sizeof(envelope));
    for (int32_t i = 0; i < 17; i++) {
        f->effects[i] = (MDRV_EFFECT) { .channel_count = 1,
                                        .base_channel = 0x40,
                                        .stream_offsets = { 88 } };
    }
    f->assets.effects = (MDRV_EFFECTS) { .bytes = f->effect_bytes,
                                         .size = 118,
                                         .ssg_offset = 112,
                                         .ssg_count = 1,
                                         .effects = f->effects,
                                         .effect_count = 17 };
    memcpy(f->music_bytes, (const uint8_t[]) { 0x90, 255, 0xff }, 3);
    f->track = (MDRV_TRACK) { .channel = 0x80 };
    for (int32_t i = 0; i < GAMEPLAY_SONG_COUNT; i++) {
        f->assets.songs[i] = (MDRV_MUSIC) { .bytes = f->music_bytes,
                                            .size = 3,
                                            .fm_offset = 3,
                                            .tracks = &f->track,
                                            .track_count = 1 };
    }
    M_BossAssets(f, a);
    return &f->assets;
}
