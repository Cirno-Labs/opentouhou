#pragma once

#include "core/base/result.h"

#include <stddef.h>

// Paused fork()ed copies of the recorder, so a run continues from a copy
// instead of replaying from the start. Each snapshot serves requests on a
// socket whose only other end the recorder holds, so snapshots exit with it.
// Without Linux, SNAPSHOT_AVAILABLE is 0 and Snapshot_Start fails.

#if defined(__linux__)
    #define SNAPSHOT_AVAILABLE 1
#else
    #define SNAPSHOT_AVAILABLE 0
#endif

// Refers to a snapshot from the recorder, by its socket, or -1 for none.
typedef struct {
    int fd;
} SNAPSHOT;

// Holds a copy's request state while it serves.
typedef struct {
    int fd;
    int kept;
    int kept_pid;
    bool replied;
} SNAPSHOT_COPY;

// Serves size bytes of request in a copy, which replies with
// Snapshot_Reply. Returns true only in a snapshot that Snapshot_Keep made,
// which must return at once.
typedef bool (*SNAPSHOT_SERVE)(
    void *context, const void *request, size_t size, SNAPSHOT_COPY *copy);

// Makes the recorder the reaper of every snapshot and forks the first one
// at the current state. Every later snapshot comes from a request.
RESULT Snapshot_Start(SNAPSHOT *out, SNAPSHOT_SERVE serve, void *context);

// Sends a request to snapshot and writes the copy's reply, of at most
// reply_size bytes, into reply. Writes the snapshot that the copy kept into
// kept, or one of fd -1. Reports failure when the copy failed.
RESULT Snapshot_Request(
    SNAPSHOT snapshot, const void *request, size_t request_size, void *reply,
    size_t reply_size, SNAPSHOT *kept);

// Closes snapshot, whose process then exits, and sets it to none.
void Snapshot_Close(SNAPSHOT *snapshot);

// Waits for every snapshot process to exit, once all are closed.
void Snapshot_Shutdown(void);

// Forks the copy's state as a new snapshot and returns true in it, false in
// the copy. A copy keeps at most one.
bool Snapshot_Keep(SNAPSHOT_COPY *copy);

// Ends the snapshot that the copy kept.
void Snapshot_Drop(SNAPSHOT_COPY *copy);

// Sends size bytes of reply to the recorder, with the snapshot that the
// copy kept.
void Snapshot_Reply(SNAPSHOT_COPY *copy, const void *reply, size_t size);
