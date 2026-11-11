#include "core/patch.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/json.h"
#include "core/vfs.h"

#include "patch_builder.h"
#include "test.h"
#include "test_allocator.h"
#include "test_json.h"

#include <string.h>

static PATCH_FILE_KIND M_Classify(const char *const name)
{
    return Str_EndsWith(name, ".png") ? PATCH_FILE_IGNORED : PATCH_FILE_DATA;
}

static PATCH_STACK *M_Open(const char *const dir, const PATCH_LAYOUT layout)
{
    const PATCH_GAME game = {
        .build = "v1.02h",
        .layout = layout,
        .classify = M_Classify,
    };
    PATCH_STACK *stack = nullptr;
    CHECK(Result_Log(
        Patch_OpenStack(dir, "th06", &game, nullptr, 0, &stack),
        LOG_LEVEL_ERROR));
    return stack;
}

// Returns the content of path in vfs, opened in mode, as a new string that
// Mem_Free releases, or nullptr when it does not open.
static char *M_Read(VFS *const vfs, const char *const path, const VFS_MODE mode)
{
    READER reader;
    if (!Result_Ignore(Vfs_Open(vfs, path, mode, &reader))) {
        return nullptr;
    }
    const size_t size = Reader_Size(&reader);
    char *const text = Mem_AllocZ(size + 1);
    Reader_ReadBytes(&reader, text, size);
    Reader_Close(&reader);
    return text;
}

static void M_CheckRead(
    VFS *const vfs, const char *const path, const char *const expected)
{
    char *const text = M_Read(vfs, path, VFS_IN_MEMORY);
    CHECK_EQ_STR(text, expected);
    Mem_Free(text);
    char *const streamed = M_Read(vfs, path, VFS_STREAM);
    CHECK_EQ_STR(streamed, expected);
    Mem_Free(streamed);
}

// Mounts the data directory data at priority 0 and the patches of stack
// at priority 100.
static VFS *M_Mount(
    const char *const data, const PATCH_STACK *const stack,
    const ALLOCATOR *const allocator)
{
    VFS *const vfs = Vfs_Create(allocator);
    CHECK(Result_Log(Vfs_MountDir(vfs, data, 0), LOG_LEVEL_ERROR));
    Vfs_MountSource(vfs, Patch_CreateSource(stack, allocator), 100);
    return vfs;
}

TEST(a_patch_file_replaces_the_game_file_in_patch_order)
{
    char *const dir = TestDir_Prepare("overlay");
    PatchBuilder_Write(dir, "data/data/msg1.dat", "original");
    PatchBuilder_Write(dir, "data/data/msg2.dat", "original 2");
    PatchBuilder_AddPatch(dir, "patches/a/base", "{}");
    PatchBuilder_AddPatch(dir, "patches/a/top", "{dependencies: ['base']}");
    PatchBuilder_Write(dir, "patches/a/base/th06/msg1.dat", "base");
    PatchBuilder_Write(dir, "patches/a/top/th06/MSG1.DAT", "top");
    PatchBuilder_Write(dir, "patches/a/base/th06/msg3.dat", "new");

    char *const data = TestDir_Join(dir, "data");
    char *const patches = TestDir_Join(dir, "patches");
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    PATCH_STACK *const stack = M_Open(patches, PATCH_LAYOUT_FLAT);
    VFS *const vfs = M_Mount(data, stack, &test.allocator);
    M_CheckRead(vfs, "data/msg1.dat", "top");
    M_CheckRead(vfs, "data/msg2.dat", "original 2");
    M_CheckRead(vfs, "data/msg3.dat", "new");
    Vfs_Destroy(vfs);
    Patch_CloseStack(stack);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(patches);
    Mem_Free(data);
    Mem_Free(dir);
}

TEST(a_build_specific_name_beats_the_plain_one_in_the_same_patch)
{
    char *const dir = TestDir_Prepare("build");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['base']}");
    PatchBuilder_Write(dir, "a/base/th06/msg1.dat", "base plain");
    PatchBuilder_Write(dir, "a/base/th06/msg1.v1.02h.dat", "base build");
    PatchBuilder_Write(dir, "a/base/th06/msg2.V1.02H.dat", "base build 2");
    PatchBuilder_Write(dir, "a/top/th06/msg2.dat", "top plain");
    PatchBuilder_Write(dir, "a/base/th06/msg3.v1.00.dat", "other build");
    PatchBuilder_Write(dir, "a/base/th06/end", "plain");
    PatchBuilder_Write(dir, "a/base/th06/end.v1.02h", "build");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(vfs, Patch_CreateSource(stack, &g_HeapAllocator), 0);
    M_CheckRead(vfs, "msg1.dat", "base build");
    M_CheckRead(vfs, "msg2.dat", "top plain");
    M_CheckRead(vfs, "end", "build");
    CHECK(!Vfs_Exists(vfs, "msg3.dat"));
    CHECK(Vfs_Exists(vfs, "msg3.v1.00.dat"));
    Vfs_Destroy(vfs);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(the_flat_layout_finds_a_file_by_its_last_component)
{
    char *const dir = TestDir_Prepare("flat");
    PatchBuilder_AddPatch(dir, "a/p", "{}");
    PatchBuilder_Write(dir, "a/p/th06/title00.jpg", "flat");
    PatchBuilder_Write(dir, "a/p/th06/data/nested.jpg", "nested");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    CHECK(Patch_Touches(stack, "data\\title\\Title00.jpg"));
    CHECK(!Patch_Touches(stack, "data/nested.jpg"));
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(the_paths_layout_finds_a_file_by_its_path)
{
    char *const dir = TestDir_Prepare("paths");
    PatchBuilder_AddPatch(dir, "a/p", "{}");
    PatchBuilder_Write(dir, "a/p/th06/data/Front/front.anm", "deep");
    PatchBuilder_Write(dir, "a/p/th06/front.anm", "top");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_PATHS);
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(vfs, Patch_CreateSource(stack, &g_HeapAllocator), 0);
    M_CheckRead(vfs, "data/front/front.anm", "deep");
    M_CheckRead(vfs, "front.anm", "top");
    CHECK(!Vfs_Exists(vfs, "data/front.anm"));
    Vfs_Destroy(vfs);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(ignored_and_jdiff_files_are_not_overlaid)
{
    char *const dir = TestDir_Prepare("not_overlaid");
    PatchBuilder_AddPatch(dir, "a/p", "{}");
    PatchBuilder_Write(dir, "a/p/th06/front.png", "image");
    PatchBuilder_Write(dir, "a/p/th06/msg1.dat.jdiff", "{}");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(vfs, Patch_CreateSource(stack, &g_HeapAllocator), 0);
    CHECK(!Vfs_Exists(vfs, "front.png"));
    CHECK(!Vfs_Exists(vfs, "msg1.dat.jdiff"));
    CHECK(!Patch_Touches(stack, "front.png"));
    CHECK(Patch_Touches(stack, "data/msg1.dat"));
    CHECK(!Patch_Touches(stack, "data/msg2.dat"));
    Vfs_Destroy(vfs);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

static void M_CollectPath(void *const user, const char *const path)
{
    char **const joined = user;
    char *const next = Str_Format("%s[%s]", *joined, path);
    Mem_Free(*joined);
    *joined = next;
}

TEST(the_source_lists_each_overlaid_file_once)
{
    char *const dir = TestDir_Prepare("list");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['base']}");
    PatchBuilder_Write(dir, "a/base/th06/msg1.dat", "x");
    PatchBuilder_Write(dir, "a/top/th06/msg1.dat", "x");
    PatchBuilder_Write(dir, "a/top/th06/front.png", "x");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    VFS_SOURCE *const source = Patch_CreateSource(stack, &g_HeapAllocator);
    char *joined = Str_Dup("");
    source->funcs->list(source, M_CollectPath, &joined);
    CHECK_EQ_STR(joined, "[msg1.dat]");
    Mem_Free(joined);
    source->funcs->close(source);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

// Returns the JSON text of value, compact.
static void M_Dump(
    const JSON_VALUE *const value, char *const buffer, const size_t size)
{
    size_t used = 0;
    M_TestJsonDump(value, buffer, size, &used);
}

TEST(json_files_merge_in_patch_order_with_the_build_last)
{
    char *const dir = TestDir_Prepare("json");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['base']}");
    PatchBuilder_Write(dir, "a/base/stringdefs.js", "{a: 'base', b: 'base'}");
    PatchBuilder_Write(dir, "a/base/stringdefs.v1.02h.js", "{b: 'build'}");
    PatchBuilder_Write(dir, "a/top/stringdefs.js", "{a: 'top', c: {d: 1}}");
    PatchBuilder_Write(dir, "a/top/th06/spells.js", "{'0': 'Moon'}");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 20);
    char buffer[256];
    M_Dump(
        Patch_LoadJson(
            stack, PATCH_ROOT, "stringdefs.js", Arena_Allocator(&arena)),
        buffer, sizeof(buffer));
    CHECK_EQ_STR(buffer, "{\"a\":\"top\",\"b\":\"build\",\"c\":{\"d\":1}}");
    M_Dump(
        Patch_LoadJson(
            stack, PATCH_GAME_DIR, "Spells.js", Arena_Allocator(&arena)),
        buffer, sizeof(buffer));
    CHECK_EQ_STR(buffer, "{\"0\":\"Moon\"}");
    CHECK(
        Patch_LoadJson(stack, PATCH_ROOT, "spells.js", Arena_Allocator(&arena))
        == nullptr);
    CHECK(
        Patch_LoadJson(
            nullptr, PATCH_ROOT, "spells.js", Arena_Allocator(&arena))
        == nullptr);
    Arena_Free(&arena);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_broken_json_file_is_left_out_of_the_merge)
{
    char *const dir = TestDir_Prepare("json_broken");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['base']}");
    PatchBuilder_Write(dir, "a/base/themes.js", "{a: 1}");
    PatchBuilder_Write(dir, "a/top/themes.js", "{a: ");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 20);
    char buffer[64];
    M_Dump(
        Patch_LoadJson(stack, PATCH_ROOT, "themes.js", Arena_Allocator(&arena)),
        buffer, sizeof(buffer));
    CHECK_EQ_STR(buffer, "{\"a\":1}");
    Arena_Free(&arena);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}

TEST(a_diff_merges_the_jdiff_of_the_game_file)
{
    char *const dir = TestDir_Prepare("diff");
    PatchBuilder_AddPatch(dir, "a/base", "{}");
    PatchBuilder_AddPatch(dir, "a/top", "{dependencies: ['base']}");
    PatchBuilder_Write(
        dir, "a/base/th06/msg1.dat.jdiff", "{'0': {'1_0': ['a', 'b']}}");
    PatchBuilder_Write(
        dir, "a/top/th06/msg1.v1.02h.dat.jdiff", "{'0': {'2_0': ['c']}}");
    PATCH_STACK *const stack = M_Open(dir, PATCH_LAYOUT_FLAT);
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 20);
    char buffer[128];
    M_Dump(
        Patch_LoadDiff(stack, "data/msg1.dat", Arena_Allocator(&arena)), buffer,
        sizeof(buffer));
    CHECK_EQ_STR(buffer, "{\"0\":{\"1_0\":[\"a\",\"b\"],\"2_0\":[\"c\"]}}");
    CHECK(
        Patch_LoadDiff(stack, "data/msg2.dat", Arena_Allocator(&arena))
        == nullptr);
    Arena_Free(&arena);
    Patch_CloseStack(stack);
    Mem_Free(dir);
}
