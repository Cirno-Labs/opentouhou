#include "th02/program/private.h"

#include "core/vfs.h"
#include "pc98/zun/pf.h"

RESULT ProgramAssets_Mount(VFS *const files)
{
    READER reader;
    MUST(Vfs_Open(files, "東方封魔.録", VFS_STREAM, &reader));
    VFS_SOURCE *source;
    MUST(Pf_Open(&reader, PF_KEY_12, "東方封魔.録", &g_HeapAllocator, &source));
    Vfs_MountSource(files, "", source, 10);
    return OK;
}

RESULT ProgramAssets_Load(VFS *const files)
{
    MUST(Demo_Load(files, p_Program->assets.demos));
    RESULT result = OpeningAssets_Load(
        files, &p_Program->display.font,
        Arena_Allocator(&p_Program->assets.arena), &p_Program->assets.opening);
    if (IS_OK(result)) {
        result = GameplayAssets_Load(
            files, Arena_Allocator(&p_Program->assets.arena),
            &p_Program->assets.gameplay);
    }
    if (IS_OK(result)) {
        for (uint8_t shot = 0; IS_OK(result) && shot < 3; shot++) {
            result = BombAssets_Load(
                files, shot, Arena_Allocator(&p_Program->assets.arena),
                &p_Program->assets.bombs[shot]);
        }
        if (IS_OK(result)) {
            result = MarisaAssets_Load(
                files, Arena_Allocator(&p_Program->assets.arena),
                &p_Program->assets.gameplay.bosses.marisa);
        }
        if (IS_OK(result)) {
            result = MimaAssets_Load(
                files, Arena_Allocator(&p_Program->assets.arena),
                &p_Program->assets.gameplay.bosses.mima);
        }
        if (IS_OK(result)) {
            result = SigmaAssets_Load(
                files, Arena_Allocator(&p_Program->assets.arena),
                &p_Program->assets.gameplay.bosses.sigma);
        }
        for (uint8_t stage = 0; IS_OK(result) && stage < 6; stage++) {
            p_Program->assets.stage_patterns[stage] =
                p_Program->assets.gameplay.patterns;
            result = StageAssets_Load(
                files, stage, Arena_Allocator(&p_Program->assets.arena),
                &p_Program->assets.stage_patterns[stage],
                &p_Program->assets.stages[stage]);
            if (IS_OK(result)) {
                result = DialogueAssets_Load(
                    files, stage, Arena_Allocator(&p_Program->assets.arena),
                    &p_Program->assets.gameplay.bosses.stages[stage]);
            }
        }
    }
    if (IS_OK(result)) {
        const DRAW_TEXT_FONT font = TextFont_Graphics(&p_Program->display.font);
        result = EndingAssets_Load(
            files, &font, Arena_Allocator(&p_Program->assets.arena),
            &p_Program->assets.ending);
    }
    if (IS_OK(result)) {
        result = MusicAssets_Load(
            files, Arena_Allocator(&p_Program->assets.arena),
            &p_Program->assets.music);
    }
    return result;
}
