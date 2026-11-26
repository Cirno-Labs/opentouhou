#include "snapshot.h"

#if SNAPSHOT_AVAILABLE

    #include "core/base/memory.h"

    #include <errno.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>
    #include <sys/prctl.h>
    #include <sys/socket.h>
    #include <sys/wait.h>
    #include <unistd.h>

// Leads each reply. A failed copy has status 1 and no payload.
typedef struct {
    uint64_t status;
    uint64_t size;
} M_REPLY;

static SNAPSHOT_SERVE m_Serve;
static void *m_Context;

static bool M_Write(const int fd, const void *const data, const size_t size)
{
    const uint8_t *at = data;
    for (size_t left = size; left > 0;) {
        const ssize_t count = send(fd, at, left, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        at += count;
        left -= (size_t)count;
    }
    return true;
}

static bool M_Read(const int fd, void *const data, const size_t size)
{
    uint8_t *at = data;
    for (size_t left = size; left > 0;) {
        const ssize_t count = recv(fd, at, left, 0);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        at += count;
        left -= (size_t)count;
    }
    return true;
}

// Sends header and size bytes of payload in one message, with the socket
// kept attached when it is not -1.
static bool M_SendReply(
    const int fd, const M_REPLY header, const void *const payload,
    const int kept)
{
    struct iovec parts[2] = {
        { .iov_base = (void *)&header, .iov_len = sizeof(header) },
        { .iov_base = (void *)payload, .iov_len = (size_t)header.size },
    };
    union {
        struct cmsghdr header;
        char space[CMSG_SPACE(sizeof(int))];
    } control;
    memset(&control, 0, sizeof(control));
    struct msghdr message = {
        .msg_iov = parts,
        .msg_iovlen = header.size > 0 ? 2 : 1,
    };
    if (kept >= 0) {
        message.msg_control = control.space;
        message.msg_controllen = sizeof(control.space);
        struct cmsghdr *const item = CMSG_FIRSTHDR(&message);
        item->cmsg_level = SOL_SOCKET;
        item->cmsg_type = SCM_RIGHTS;
        item->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(item), &kept, sizeof(int));
    }
    ssize_t count;
    do {
        count = sendmsg(fd, &message, MSG_NOSIGNAL);
    } while (count < 0 && errno == EINTR);
    if (count < 0) {
        return false;
    }
    const size_t total = sizeof(header) + (size_t)header.size;
    if ((size_t)count >= total) {
        return true;
    }
    if ((size_t)count < sizeof(header)) {
        return M_Write(
                   fd, (const uint8_t *)&header + count,
                   sizeof(header) - (size_t)count)
            && M_Write(fd, payload, (size_t)header.size);
    }
    return M_Write(
        fd, (const uint8_t *)payload + ((size_t)count - sizeof(header)),
        total - (size_t)count);
}

// Reads a reply header and the socket attached to it, or -1.
static bool M_ReceiveReply(const int fd, M_REPLY *const header, int *const kept)
{
    *kept = -1;
    union {
        struct cmsghdr header;
        char space[CMSG_SPACE(sizeof(int))];
    } control;
    struct iovec part = { .iov_base = header, .iov_len = sizeof(*header) };
    struct msghdr message = {
        .msg_iov = &part,
        .msg_iovlen = 1,
        .msg_control = control.space,
        .msg_controllen = sizeof(control.space),
    };
    ssize_t count;
    do {
        count = recvmsg(fd, &message, MSG_CMSG_CLOEXEC);
    } while (count < 0 && errno == EINTR);
    if (count <= 0) {
        return false;
    }
    for (struct cmsghdr *item = CMSG_FIRSTHDR(&message); item != nullptr;
         item = CMSG_NXTHDR(&message, item)) {
        if (item->cmsg_level == SOL_SOCKET && item->cmsg_type == SCM_RIGHTS) {
            memcpy(kept, CMSG_DATA(item), sizeof(int));
        }
    }
    return M_Read(
        fd, (uint8_t *)header + count, sizeof(*header) - (size_t)count);
}

// Serves requests on fd until the recorder closes it. Each request runs in
// a forked copy; the copy that Snapshot_Keep made serves its own socket.
[[noreturn]] static void M_RunSnapshot(int fd)
{
    uint8_t *request = nullptr;
    uint64_t size;
    while (M_Read(fd, &size, sizeof(size))) {
        request = Mem_Realloc(request, size > 0 ? (size_t)size : 1);
        if (!M_Read(fd, request, (size_t)size)) {
            break;
        }
        const pid_t pid = fork();
        if (pid == 0) {
            SNAPSHOT_COPY copy = { .fd = fd, .kept = -1, .kept_pid = -1 };
            if (m_Serve(m_Context, request, (size_t)size, &copy)) {
                fd = copy.fd;
                continue;
            }
            _exit(copy.replied ? 0 : 1);
        }
        int status = 0;
        while (pid > 0 && waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
        if (pid < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            const M_REPLY failed = { .status = 1 };
            if (!M_SendReply(fd, failed, nullptr, -1)) {
                break;
            }
        }
    }
    _exit(0);
}

static void M_Reap(void)
{
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
}

RESULT Snapshot_Start(
    SNAPSHOT *const out, const SNAPSHOT_SERVE serve, void *const context)
{
    out->fd = -1;
    FAIL_IF(
        prctl(PR_SET_CHILD_SUBREAPER, 1) != 0,
        "Cannot become the reaper of the snapshots: %s", strerror(errno));
    m_Serve = serve;
    m_Context = context;
    int sockets[2];
    FAIL_IF(
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0,
        "Cannot open a snapshot socket: %s", strerror(errno));
    fflush(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        close(sockets[0]);
        M_RunSnapshot(sockets[1]);
    }
    close(sockets[1]);
    if (pid < 0) {
        close(sockets[0]);
        return FAIL("Cannot fork a snapshot: %s", strerror(errno));
    }
    out->fd = sockets[0];
    return OK;
}

RESULT Snapshot_Request(
    const SNAPSHOT snapshot, const void *const request,
    const size_t request_size, void *const reply, const size_t reply_size,
    SNAPSHOT *const kept)
{
    kept->fd = -1;
    M_Reap();
    const uint64_t size = request_size;
    FAIL_IF(
        !M_Write(snapshot.fd, &size, sizeof(size))
            || !M_Write(snapshot.fd, request, request_size),
        "Cannot send a request to a snapshot: %s", strerror(errno));
    M_REPLY header;
    int fd;
    FAIL_IF(
        !M_ReceiveReply(snapshot.fd, &header, &fd),
        "A snapshot closed without a reply");
    const bool read = header.status == 0 && header.size == reply_size
        && M_Read(snapshot.fd, reply, reply_size);
    if (!read && fd >= 0) {
        close(fd);
    }
    FAIL_IF(!read, "A snapshot's copy failed");
    kept->fd = fd;
    return OK;
}

void Snapshot_Close(SNAPSHOT *const snapshot)
{
    if (snapshot->fd >= 0) {
        close(snapshot->fd);
    }
    snapshot->fd = -1;
}

void Snapshot_Shutdown(void)
{
    while (waitpid(-1, nullptr, 0) > 0 || errno == EINTR) {}
}

bool Snapshot_Keep(SNAPSHOT_COPY *const copy)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) {
        _exit(1);
    }
    const pid_t pid = fork();
    if (pid < 0) {
        _exit(1);
    }
    if (pid == 0) {
        close(copy->fd);
        close(sockets[0]);
        *copy =
            (SNAPSHOT_COPY) { .fd = sockets[1], .kept = -1, .kept_pid = -1 };
        return true;
    }
    close(sockets[1]);
    copy->kept = sockets[0];
    copy->kept_pid = pid;
    return false;
}

void Snapshot_Drop(SNAPSHOT_COPY *const copy)
{
    if (copy->kept < 0) {
        return;
    }
    close(copy->kept);
    while (waitpid(copy->kept_pid, nullptr, 0) < 0 && errno == EINTR) {}
    copy->kept = -1;
    copy->kept_pid = -1;
}

void Snapshot_Reply(
    SNAPSHOT_COPY *const copy, const void *const reply, const size_t size)
{
    const M_REPLY header = { .size = size };
    copy->replied = M_SendReply(copy->fd, header, reply, copy->kept);
    if (copy->kept >= 0) {
        close(copy->kept);
        copy->kept = -1;
    }
}

#else

RESULT Snapshot_Start(
    SNAPSHOT *const out, const SNAPSHOT_SERVE serve, void *const context)
{
    out->fd = -1;
    return FAIL("Snapshots need fork");
}

RESULT Snapshot_Request(
    const SNAPSHOT snapshot, const void *const request,
    const size_t request_size, void *const reply, const size_t reply_size,
    SNAPSHOT *const kept)
{
    kept->fd = -1;
    return FAIL("Snapshots need fork");
}

void Snapshot_Close(SNAPSHOT *const snapshot)
{
    snapshot->fd = -1;
}

void Snapshot_Shutdown(void)
{
}

bool Snapshot_Keep(SNAPSHOT_COPY *const copy)
{
    return false;
}

void Snapshot_Drop(SNAPSHOT_COPY *const copy)
{
}

void Snapshot_Reply(
    SNAPSHOT_COPY *const copy, const void *const reply, const size_t size)
{
    copy->replied = true;
}

#endif
