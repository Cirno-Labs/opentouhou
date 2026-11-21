#include "th01/gameplay_assets.h"

#include "th01/items.h"
#include "th01/miss.h"

#define M_LOAD(name, loader, out)                                              \
    do {                                                                       \
        READER asset_reader;                                                   \
        MUST(Vfs_Open(vfs, name, VFS_IN_MEMORY, &asset_reader));               \
        const RESULT asset_result = loader(&asset_reader, allocator, out);     \
        Reader_Close(&asset_reader);                                           \
        MUST(asset_result);                                                    \
    } while (false)

static RESULT M_Fonts(
    const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    GAMEPLAY_ASSETS *const out)
{
    MUST(Hud_LoadFont(rom, allocator, &out->hud_font));
    MUST(Items_LoadFont(rom, allocator, &out->item_font));
    MUST(Bonus_LoadFont(rom, allocator, &out->bonus_font));
    MUST(RouteSelect_LoadFont(rom, allocator, &out->route_font));
    return GameText_Load(
        rom,
        "0123456789０１２３４５６７８９ "
        "STAGEHARRYUP東方★靈異伝ＰＡＵＳＥ再開終了本当にしちゃうの？　うそ"
        "ですぅはいっ●ｃｏｎｔｉｎｕｅＹｅｓＮｏ",
        allocator, &out->menu_font);
}

RESULT GameplayAssets_Load(
    VFS *const vfs, const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    GAMEPLAY_ASSETS *const out)
{
    *out = (GAMEPLAY_ASSETS) {};
    uint8_t *const scratch = Allocator_Alloc(allocator, REIIDEN_IMAGE_SIZE);
    for (uint8_t i = 0; i < SCENE_COUNT; i++) {
        MUST(Scene_Load(vfs, i, allocator, &out->scenes[i]));
        M_LOAD(out->scenes[i].background, Pi_Load, &out->backgrounds[i]);
    }
    M_LOAD("STG.PTN", Ptn_Load, &out->stage);
    M_LOAD("STG_B.PTN", Ptn_Load, &out->boss_stage);
    M_LOAD("MIKO.PTN", Ptn_Load, &out->miko);
    M_LOAD("MIKO_AC.BOS", Bos_Load, &out->miko_ac);
    M_LOAD("MIKO_AC2.BOS", Bos_Load, &out->miko_ac2);
    M_LOAD("MASK.GRF", Grf_Load, &out->hud);
    MUST(Bomb_LoadAssets(vfs, allocator, &out->bomb));
    MUST(B05_LoadAssets(vfs, allocator, &out->b05));
    MUST(B10J_LoadAssets(
        vfs, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->b10j));
    MUST(B10M_LoadAssets(vfs, allocator, &out->b10m));
    MUST(B15J_LoadAssets(vfs, allocator, &out->b15j));
    MUST(B15M_LoadAssets(
        vfs, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->b15m));
    MUST(B20J_LoadAssets(
        vfs, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->b20j));
    MUST(B20M_LoadAssets(
        vfs, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->b20m));
    MUST(Bonus_LoadTotalAssets(vfs, allocator, &out->total));
    READER reader;
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    RESULT result = PelletAssets_Load(
        &reader, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->pellets);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    result = ShootoutLasers_LoadAssets(
        &reader, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->lasers);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    result = Bonus_LoadStageAssets(
        &reader, scratch, REIIDEN_IMAGE_SIZE, allocator, &out->bonus);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "REIIDEN.EXE", VFS_IN_MEMORY, &reader));
    result = Invincibility_LoadAssets(
        &reader, scratch, REIIDEN_IMAGE_SIZE, &out->invincibility);
    Reader_Close(&reader);
    MUST(result);
    MUST(M_Fonts(rom, allocator, out));
    const char *const songs[GAMEPLAY_SONG_COUNT] = {
        "ST0.MDT",   "ST1.MDT",     "ST2.MDT",      "ST3.MDT",    "ST4.MDT",
        "ST5.MDT",   "ST6.MDT",     "POSITIVE.MDT", "LEGEND.MDT", "KAMI.MDT",
        "KAMI2.MDT", "INIT.MDT",    "ALICE.MDT",    "TENSI.MDT",  "SYUGEN.MDT",
        "REIMU.MDT", "ZIPANGU.MDT", "IRIS.MDT",
    };
    for (int32_t i = 0; i < GAMEPLAY_SONG_COUNT; i++) {
        M_LOAD(songs[i], MdrvData_LoadMusic, &out->songs[i]);
    }
    M_LOAD("ZIGOKU.MDE", MdrvData_LoadEffects, &out->effects);
    const PLAYER_SPRITES player = {
        .miko = &out->miko,
        .miko_ac = &out->miko_ac,
        .miko_ac2 = &out->miko_ac2,
    };
    MUST(Player_ValidateSprites(&player));
    MUST(Miss_ValidateSprites(&out->miko, &out->stage));
    MUST(StageObjects_ValidateSprites(&out->stage, 0));
    MUST(StageObjects_ValidateSprites(&out->boss_stage, 4));
    MUST(Stage_ValidateScoreFont(&out->hud_font));
    MUST(Hud_ValidateAssets(&out->hud, &out->stage, &out->hud_font));
    MUST(Items_ValidateAssets(&out->stage, &out->item_font));
    MUST(Bonus_ValidateStageAssets(out->bonus, &out->bonus_font));
    return OK;
}
