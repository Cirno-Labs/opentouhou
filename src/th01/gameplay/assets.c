#include "th01/gameplay/assets.h"

#include "th01/boss/strategy.h"
#include "th01/items.h"
#include "th01/miss.h"
#include "th01/route_select.h"
#include "th01/shots.h"

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
    for (BOSS boss = BOSS_SINGYOKU; boss < BOSS_COUNT; boss++) {
        const BOSS_STRATEGY *const strategy = BossStrategy_Get(boss);
        void *const assets = Allocator_AllocZ(allocator, strategy->assets_size);
        MUST(strategy->load_assets(vfs, allocator, assets));
        out->bosses[boss] = assets;
    }
    MUST(Bonus_LoadTotalAssets(vfs, allocator, &out->total));
    MUST(PelletAssets_Load(vfs, allocator, &out->pellets));
    MUST(ShootoutLasers_LoadAssets(vfs, allocator, &out->lasers));
    MUST(Bonus_LoadStageAssets(vfs, allocator, &out->bonus));
    MUST(Invincibility_LoadAssets(vfs, &out->invincibility));
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
    MUST(Shots_ValidateSprites(&out->stage));
    MUST(Shots_ValidateSprites(&out->boss_stage));
    const ROUTE_SELECT_ASSETS route = { .font = &out->route_font,
                                        .stage = &out->stage,
                                        .makai = &out->scenes[1],
                                        .jigoku = &out->scenes[2] };
    MUST(RouteSelect_ValidateAssets(&route));
    MUST(Bonus_ValidateStageAssets(out->bonus, &out->bonus_font));
    return OK;
}
