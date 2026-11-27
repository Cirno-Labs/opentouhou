#include "th02/high_scores/file.h"

RESULT HighScoresFile_Save(
    VFS *const user_files, const HIGH_SCORES *const scores)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const RESULT encoded = HighScores_Write(&writer, scores);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    const RESULT closed = Writer_Close(&writer);
    if (!IS_OK(encoded)) {
        Result_Ignore(closed);
        Mem_Free(bytes);
        return encoded;
    }
    if (!IS_OK(closed)) {
        Mem_Free(bytes);
        return closed;
    }
    const RESULT result = Vfs_Write(user_files, "HUUHI.DAT", bytes, size);
    Mem_Free(bytes);
    return result;
}

RESULT HighScoresFile_Load(
    VFS *const original_files, VFS *const user_files, HIGH_SCORES *const out)
{
    const bool user_exists = Vfs_Exists(user_files, "HUUHI.DAT");
    VFS *const source = user_exists ? user_files : original_files;
    HIGH_SCORES scores;
    if (user_exists
        || (!Vfs_Exists(user_files, "config.ini")
            && Vfs_Exists(original_files, "HUUHI.DAT"))) {
        READER reader;
        MUST(Vfs_Open(source, "HUUHI.DAT", VFS_STREAM, &reader));
        const RESULT result = HighScores_Read(&reader, &scores);
        Reader_Close(&reader);
        if (!IS_OK(result)) {
            if (user_exists) {
                const RESULT removed = Vfs_Remove(user_files, "HUUHI.DAT");
                if (!IS_OK(removed)) {
                    Result_Ignore(result);
                    return removed;
                }
                Result_Ignore(result);
                return FAIL("Removed corrupt HUUHI.DAT; restart the game");
            }
            return result;
        }
    } else {
        HighScores_Defaults(&scores);
    }
    if (!user_exists) {
        MUST(HighScoresFile_Save(user_files, &scores));
    }
    *out = scores;
    return OK;
}
