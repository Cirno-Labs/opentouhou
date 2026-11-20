#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

// The namespace a game opens its files from, by their original names, such
// as "data/title00.jpg" or "bgm/th06_01.wav". Several sources are mounted
// with priorities, such as patch directories, archives and the game's data
// directory; a lookup goes to the highest-priority mounted source that has
// the file.
//
// Example:
// VFS *const vfs = Vfs_Create(&g_HeapAllocator);
// MUST(Vfs_MountDir(vfs, "", "/path/to/game", 0, false));
// Vfs_MountSource(vfs, "", my_source, 100);
// READER reader;
// MUST(Vfs_Open(vfs, "data/title00.jpg", VFS_IN_MEMORY, &reader));
// // use reader
// Reader_Close(&reader);
// Vfs_Destroy(vfs);

// Picks how Vfs_Open returns a file's data.
typedef enum {
    // The READER holds the whole file in memory: Reader_Borrow works, and
    // the pointers it returns stay valid until Reader_Close.
    VFS_IN_MEMORY,
    // The READER may stream the file from disk instead. A source that
    // cannot stream its files, such as a compressed archive entry, falls
    // back to reading them into memory, so VFS_STREAM never fails because
    // of how a file is stored.
    VFS_STREAM,
} VFS_MODE;

typedef struct VFS_SOURCE VFS_SOURCE;

// The functions a mounted source implements. contains and open receive a
// path the VFS has already normalized (core/vfs/vfs.c normalizes every path
// before it reaches a source), so a source only ever compares normalized
// paths, never a path as a game gave it. list calls visit with the name of
// each file directly below dir, a normalized path relative to the source or
// "" for its root, as the source spells it, in the source's order. write,
// nullptr in a read-only source, creates or replaces the file at path,
// which keeps the case that the game gave, with size bytes of data. close
// releases the source itself, along with anything it holds.
typedef struct {
    bool (*contains)(const VFS_SOURCE *source, const char *path);
    RESULT (*open)(
        VFS_SOURCE *source, const char *path, VFS_MODE mode, READER *out);
    void (*list)(
        const VFS_SOURCE *source, const char *dir,
        void (*visit)(void *user, const char *name), void *user);
    RESULT (*write)(
        VFS_SOURCE *source, const char *path, const void *data, size_t size);
    void (*close)(VFS_SOURCE *source);
} VFS_SOURCE_FUNCS;

// The part every source implementation shares. A source embeds this as its
// first member, so a VFS_SOURCE * also points at the whole implementation,
// and the functions above can cast it back.
//
// Example:
// typedef struct {
//     VFS_SOURCE base;
//     // the implementation's own fields
// } MY_SOURCE;
//
// static bool M_Contains(const VFS_SOURCE *source, const char *path)
// {
//     const MY_SOURCE *const my_source = (const MY_SOURCE *)source;
//     // ...
// }
struct VFS_SOURCE {
    const VFS_SOURCE_FUNCS *funcs;
    // Names the source in messages, such as "紅魔郷CM.DAT".
    const char *name;
};

// Holds the sources mounted for one game. Nothing about a VFS is global, so
// a game that mounts several (not a real need yet, but nothing prevents it)
// keeps one each.
typedef struct VFS VFS;
