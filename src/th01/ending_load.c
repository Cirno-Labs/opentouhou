#include "th01/ending.h"

#include "stg/game_font.h"

RESULT Ending_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator, ENDING_ASSETS *const out)
{
    *out = (ENDING_ASSETS) {};
    static const char *const names[ENDING_SHEET_COUNT] = {
        "ED1A.GRP", "ED1B.GRP", "ED1C.GRP",   "ED1D.GRP",   "ED1E.GRP",
        "ED2A.GRP", "ED3A.GRP", "ED3B.GRP",   "ED4A.GRP",   "ED5A.GRP",
        "ED5B.GRP", "ED5C.GRP", "ENDB_A.GRP", "ENDB_B.GRP", "ENDM_A.GRP",
    };
    for (int32_t i = 0; i < ENDING_SHEET_COUNT; i++) {
        READER reader;
        MUST(Vfs_Open(vfs, names[i], VFS_IN_MEMORY, &reader));
        const RESULT decoded = Pi_Load(&reader, allocator, &out->sheets[i]);
        Reader_Close(&reader);
        MUST(decoded);
        FAIL_IF(
            out->sheets[i].width != 640 || out->sheets[i].height != 400,
            "Ending sheet must be a 640 by 400 image");
    }
    MUST(Text_Init());
    FONT *font = nullptr;
    RESULT result = GameFont_Open(nullptr, 16, &font);
    if (IS_OK(result)) {
        static const char characters[] =
            " !\"#$%&'()*+,-./"
            "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
            "abcdefghijklmnopqrstuvwxyz{|}~"
            "★　あおかたちっなのみるをんアィイエキクゲコサザスタッテトドナニハ"
            "パピホマミムモュラリルンヴー世丹九人今伝似号合回地堕墟子実帝度廃得"
            "数断方易最東林様樹檎殿炎点猿獄界異社神祠禁称終経総腐葉超越難靈静非"
            "面類高魔黄鼎（）０１２５６～";
        result = GameText_Load(font, characters, allocator, &out->font);
    }
    Font_Free(font);
    Text_Shutdown();
    return result;
}
