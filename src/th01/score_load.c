#include "th01/score.h"

#include "stg/game_font.h"

#include <string.h>

RESULT Score_LoadAssets(
    VFS *const vfs, const SCORE_SAVES *const saves,
    const ALLOCATOR *const allocator, SCORE_ASSETS *const out)
{
    *out = (SCORE_ASSETS) {};
    READER reader;
    MUST(Vfs_Open(vfs, "GAME_O.GRP", VFS_IN_MEMORY, &reader));
    const RESULT image = Pi_Load(&reader, allocator, &out->gameover);
    Reader_Close(&reader);
    MUST(image);
    FAIL_IF(
        out->gameover.width != VIDEO_WIDTH
            || out->gameover.height != VIDEO_HEIGHT,
        "GAME_O.GRP must be a 640 by 400 image");
    char characters[8192] =
        "東方靈異伝　強者の記録イージーノーマルハードルナティック"
        "霊位名前得点ステージ・ルート靈神太元帥明王天仙地人女陰陽師媒修業者"
        "＿＿＿＿＿＿＿＿魔界地獄－社無完 "
        "ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ"
        "ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ"
        "０１２３４５６７８９！？＃＆＊§☆◆∞∴♂♀…“”／．・SP←→終";
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        for (int32_t i = 0; i < SCORE_PLACES; i++) {
            strcat(characters, saves->tables[rank].entries[i].name);
            strcat(characters, saves->tables[rank].entries[i].route);
        }
    }
    MUST(Text_Init());
    FONT *font = nullptr;
    RESULT result = GameFont_Open(nullptr, 16, &font);
    if (IS_OK(result)) {
        result = GameText_Load(font, characters, allocator, &out->font);
    }
    Font_Free(font);
    Text_Shutdown();
    return result;
}
