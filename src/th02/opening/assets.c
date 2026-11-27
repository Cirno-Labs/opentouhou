#include "th02/opening/assets.h"

#include "th02/tram.h"

#include "core/base/cp932.h"
#include "pc98/zun/pi.h"

#include <string.h>

static RESULT M_Bfnt(
    VFS *const files, const char *const path, const ALLOCATOR *const allocator,
    BFNT *const out)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_STREAM, &reader));
    const RESULT result = Bfnt_Load(&reader, allocator, out);
    Reader_Close(&reader);
    return result;
}

// Reads CP932 for text into a text RAM field, with the zero terminator
// as part of the field when terminated.
static RESULT M_Text(
    const char *const text, const bool terminated,
    const ALLOCATOR *const allocator, TRAM_TEXT *const out)
{
    char *bytes;
    MUST(Cp932_Encode(text, strlen(text), &bytes));
    const size_t size = strlen(bytes) + (terminated ? 1 : 0);
    READER reader;
    Reader_OpenBuffer(&reader, (const uint8_t *)bytes, size, nullptr);
    const RESULT result = TramText_Read(&reader, size, allocator, out);
    Reader_Close(&reader);
    Mem_Free(bytes);
    return result;
}

// Reads CP932 for text into a graphics text field.
static RESULT M_GraphicsText(
    const char *const text, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, DRAW_TEXT *const out)
{
    char *bytes;
    MUST(Cp932_Encode(text, strlen(text), &bytes));
    const size_t size = strlen(bytes);
    READER reader;
    Reader_OpenBuffer(&reader, (const uint8_t *)bytes, size, nullptr);
    const RESULT result = DrawText_Read(&reader, size, font, allocator, out);
    Reader_Close(&reader);
    Mem_Free(bytes);
    return result;
}

static RESULT M_Image(
    VFS *const files, const char *const path, const ALLOCATOR *const allocator,
    TITLE_IMAGE *const out)
{
    READER reader;
    MUST(Vfs_Open(files, path, VFS_STREAM, &reader));
    PI_IMAGE image;
    const RESULT result = Pi_Load(&reader, allocator, &image);
    Reader_Close(&reader);
    MUST(result, "%s", path);
    FAIL_IF(
        image.width > VIDEO_WIDTH || image.height > VIDEO_HEIGHT
            || (image.width & 1) != 0,
        "Unsupported opening image dimensions: %s", path);
    out->view = (TITLE_FLASH_IMAGE) { image.pixels, (int16_t)image.width,
                                      (int16_t)image.height };
    memcpy(out->palette, image.palette, sizeof(out->palette));
    return OK;
}

// The score view's header from OP.EXE: "お名前 得点 STAGE TYPE 日付". Its
// zero terminator is part of the field, which reads past zeros.
static const char m_Header[] =
    "      お名前　　　　　　得点　　　 STAGE  TYPE   日付";

// The score view's shot type names from OP.EXE.
static const char *const m_ShotTypes[] = {
    "高機動",
    "防御",
    "攻撃",
};

static RESULT M_Scores(
    VFS *const files, const ALLOCATOR *const allocator,
    HIGH_SCORE_VIEW_ASSETS *const assets)
{
    BFNT sprites;
    MUST(M_Bfnt(files, "op_h.bft", &g_HeapAllocator, &sprites));
    if (sprites.width != 64 || sprites.height != 32 || sprites.count != 2
        || sprites.bits_per_pixel != 4) {
        Mem_Free((void *)sprites.pixels);
        return FAIL("op_h.bft must contain two 64 by 32 sprites");
    }
    PATTERN_BANK bank;
    PatternBank_Init(&bank, allocator);
    const RESULT registered = PatternBank_AppendBfnt(&bank, &sprites);
    Mem_Free((void *)sprites.pixels);
    MUST(registered);
    memcpy(assets->logo, bank.entries, sizeof(assets->logo));

    uint8_t colors[16][3];
    Palette_DefaultColors(colors);
    READER rgb;
    MUST(Vfs_Open(files, "op_h.rgb", VFS_STREAM, &rgb));
    const RESULT loaded = Palette_ReadRgb(&rgb, colors);
    Reader_Close(&rgb);
    MUST(loaded);
    for (size_t i = 0; i < 16; i++) {
        assets->palette[i] =
            (GFX_COLOR) { colors[i][0], colors[i][1], colors[i][2], 255 };
    }
    MUST(M_Text(m_Header, true, allocator, &assets->header));
    for (size_t i = 0; i < ARRAY_SIZE(m_ShotTypes); i++) {
        MUST(M_Text(m_ShotTypes[i], false, allocator, &assets->shot_types[i]));
    }
    return OK;
}

// The shot select's descriptions, prompt, Extra note and cleared mark from
// OP.EXE's table at 0x6f2.
static const char *const m_Shots[] = {
    " 陰陽玉の力を使わない ",
    " 広範囲でかつ機動力に ",
    "　強い高機動力タイプ　",
    " 　靈撃が優れている 　",
    "　 バランスの取れた　 ",
    "　　防御重視タイプ　　",
    " 　陰陽玉の力で戦う   ",
    "　攻撃力が優れている　",
    "　　攻撃重視タイプ　　",
    "靈夢の戦闘スタイルを、下の３つからえらんでね",
    "注）　エキストラステージでは、難易度、プレイヤー、ボム数は変更出来ません",
    "　　　それぞれ、難易度ＥＸＴＲＡ、プレイヤー３人、ボム１個となります    ",
    "  ☆☆ＣＬＥＡＲＥＤ☆☆  ",
};

static RESULT M_Shots(
    VFS *const files, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, SHOT_SELECT_ASSETS *const assets)
{
    MUST(M_Image(files, "tselect.pi", allocator, &assets->background));
    FAIL_IF(
        assets->background.view.width != VIDEO_WIDTH
            || assets->background.view.height != VIDEO_HEIGHT,
        "TSELECT.pi must be 640 by 400 pixels");
    static const char *const names[] = { "ts1.pi", "ts2.pi", "ts3.pi" };
    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        TITLE_IMAGE image;
        MUST(M_Image(files, names[i], allocator, &image));
        FAIL_IF(
            image.view.width != 192 || image.view.height != 144,
            "%s must be 192 by 144 pixels", names[i]);
        assets->portraits[i] = image.view;
    }
    DRAW_TEXT *const fields[] = {
        &assets->descriptions[0][0],
        &assets->descriptions[0][1],
        &assets->descriptions[0][2],
        &assets->descriptions[1][0],
        &assets->descriptions[1][1],
        &assets->descriptions[1][2],
        &assets->descriptions[2][0],
        &assets->descriptions[2][1],
        &assets->descriptions[2][2],
        &assets->choose,
        &assets->extra_note[0],
        &assets->extra_note[1],
        &assets->cleared,
    };
    for (size_t i = 0; i < ARRAY_SIZE(fields); i++) {
        MUST(M_GraphicsText(m_Shots[i], font, allocator, fields[i]));
    }
    return OK;
}

// The music room's 17 menu choices from OP.EXE's table at 0x8e6, 32 bytes
// each.
static const char *const m_MusicChoices[] = {
    "NO.1    東方封魔録　～浄土曼荼羅",
    "NO.2    　 博麗　～Eastern Wind ",
    "NO.3    　   End of Daylight　  ",
    "NO.4    　　　　　幻夢界　　　　",
    "NO.5    ひもろぎ、むらさきにもえ",
    "NO.6    　東方封魔録　～幽幻乱舞",
    "NO.7    　  She's in a temper!! ",
    "NO.8    　  　 やみのちから　　 ",
    "NO.9    　　　　死を賭して　　　",
    "NO.10    　  　 恋色マジック 　 ",
    "NO.11       Complete Darkness   ",
    "NO.12        　　遠野の森　　　 ",
    "NO.13       昔話わんだーらんど  ",
    "NO.14      　 エキストララブ    ",
    "NO.15      戦車むすめのみるゆめ ",
    // A blank row, then the choice that leaves the room.
    "           　　                 ",
    "           　　Ｑｕｉｔ         ",
};

static RESULT M_Music(
    VFS *const files, const DRAW_TEXT_FONT *const font,
    const ALLOCATOR *const allocator, MUSIC_ROOM_ASSETS *const assets)
{
    MUST(M_Image(files, "op3.pi", allocator, &assets->background));
    FAIL_IF(
        assets->background.view.width != VIDEO_WIDTH
            || assets->background.view.height != VIDEO_HEIGHT,
        "op3.pi must be 640 by 400 pixels");
    for (size_t i = 0; i < ARRAY_SIZE(m_MusicChoices); i++) {
        MUST(M_GraphicsText(
            m_MusicChoices[i], font, allocator, &assets->choices[i]));
    }
    READER comments;
    MUST(Vfs_Open(files, "music.txt", VFS_STREAM, &comments));
    RESULT result = OK;
    for (size_t track = 0; track < MUSIC_ROOM_TRACK_COUNT && IS_OK(result);
         track++) {
        for (size_t line = 0; line < MUSIC_ROOM_COMMENT_LINES && IS_OK(result);
             line++) {
            READER field = Reader_Slice(&comments, 42);
            result = DrawText_Read(
                &field, 40, font, allocator, &assets->comments[track][line]);
            Reader_Close(&field);
        }
    }
    Reader_Close(&comments);
    return result;
}

// The options menu's three reduce-effects labels from OP.EXE.
static const char *const m_Reduction[] = {
    "演出",
    "　通常  ",
    "一部軽減",
};

static RESULT M_Load(
    VFS *const files, TEXT_FONT *const font, const ALLOCATOR *const allocator,
    OPENING_ASSETS *const assets)
{
    MUST(M_Bfnt(files, "TOUHOU.DAT", allocator, &assets->logo));
    FAIL_IF(
        assets->logo.width != 64 || assets->logo.height != 16
            || assets->logo.count != 16 || !assets->logo.has_palette
            || assets->logo.bits_per_pixel != 4,
        "TOUHOU.DAT must contain sixteen 64 by 16 logo sprites");
    BFNT gaiji;
    MUST(M_Bfnt(files, "mikoft.bft", &g_HeapAllocator, &gaiji));
    const RESULT registered = Gaiji_Register(&assets->gaiji, &gaiji);
    Mem_Free((void *)gaiji.pixels);
    MUST(registered);
    MUST(M_Image(files, "op2.pi", allocator, &assets->title.menu_background));
    MUST(M_Image(files, "op.pi", allocator, &assets->title.title_background));
    static const char *const names[] = { "opa.pi", "opb.pi", "opc.pi" };
    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        TITLE_IMAGE image;
        MUST(M_Image(files, names[i], allocator, &image));
        assets->title.flash[i] = image.view;
    }

    uint8_t colors[16][3];
    for (size_t i = 0; i < 16; i++) {
        const GFX_COLOR color = assets->title.title_background.palette[i];
        colors[i][0] = color.red;
        colors[i][1] = color.green;
        colors[i][2] = color.blue;
    }
    READER rgb;
    MUST(Vfs_Open(files, "op.rgb", VFS_STREAM, &rgb));
    const RESULT result = Palette_ReadRgb(&rgb, colors);
    Reader_Close(&rgb);
    MUST(result);
    for (size_t i = 0; i < 16; i++) {
        assets->title.menu_palette[i] =
            (GFX_COLOR) { colors[i][0], colors[i][1], colors[i][2], 255 };
    }
    const DRAW_TEXT_FONT view = TextFont_Graphics(font);
    for (size_t i = 0; i < ARRAY_SIZE(m_Reduction); i++) {
        MUST(M_GraphicsText(
            m_Reduction[i], &view, allocator, &assets->reduction[i]));
    }
    MUST(M_Scores(files, allocator, &assets->scores));
    MUST(M_Music(files, &view, allocator, &assets->music));
    MUST(M_Shots(files, &view, allocator, &assets->shots));
    return OK;
}

RESULT OpeningAssets_Load(
    VFS *const files, TEXT_FONT *const font, const ALLOCATOR *const allocator,
    OPENING_ASSETS *const assets)
{
    memset(assets, 0, sizeof(*assets));
    OPENING_ASSETS staged;
    memset(&staged, 0, sizeof(staged));
    MUST(M_Load(files, font, allocator, &staged));
    *assets = staged;
    assets->title.glyphs = &assets->gaiji;
    return OK;
}
