#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include "zygisk_next_api.h"

static constexpr char kHostsDir[] = "/data/adb/hostsredirect";
static constexpr char kHostsPath[] = "/data/adb/hostsredirect/hosts";
static constexpr char kSelinuxCtx[] = "u:object_r:system_file:s0";

typedef union {
    struct cmsghdr cm;
    char buf[CMSG_SPACE(sizeof(int))];
} CmsgFd;
static_assert(sizeof(CmsgFd) >= CMSG_SPACE(sizeof(int)));

static void *g_handle = nullptr;
static int (*g_connect)(void *) = nullptr;
static int (*old_openat)(int, const char *, int, int) = nullptr;
static ino_t g_ino = 0;
static struct timespec g_mtime = {};

[[gnu::always_inline]] static inline bool is_target_hosts(const char *restrict p) {
    return p && p[0] == '/' &&
           (__builtin_strcmp(p + 1, "system/etc/hosts") == 0 ||
            __builtin_strcmp(p + 1, "etc/hosts") == 0);
}

static int open_prepared_hosts(void) {
    auto hfd = TEMP_FAILURE_RETRY(open(kHostsPath, O_RDONLY | O_CLOEXEC));
    if (__builtin_expect(hfd < 0, 0)) return -1;

    struct stat st = {};
    if (fstat(hfd, &st) == 0 &&
        __builtin_expect(st.st_ino != g_ino ||
                             st.st_mtim.tv_sec != g_mtime.tv_sec ||
                             st.st_mtim.tv_nsec != g_mtime.tv_nsec,
                         0)) {
        fchmod(hfd, 0o644);
        fsetxattr(hfd, "security.selinux", kSelinuxCtx, sizeof(kSelinuxCtx), 0);
        close(hfd);
        hfd = TEMP_FAILURE_RETRY(open(kHostsPath, O_RDONLY | O_CLOEXEC));
        if (__builtin_expect(hfd >= 0, 1)) {
            g_ino = st.st_ino;
            g_mtime = st.st_mtim;
        }
    }
    return hfd;
}

[[gnu::hot]] static int my_openat(int dirfd, const char *path, int flags, int mode) {
    if (__builtin_expect((flags & O_ACCMODE) != O_RDONLY || !is_target_hosts(path), 1))
        return old_openat(dirfd, path, flags, mode);

    const auto saved_errno = errno;
    const auto sfd = g_connect(g_handle);
    if (__builtin_expect(sfd >= 0, 1)) {
        char dummy = 0;
        struct iovec iov = {&dummy, 1};
        CmsgFd c = {};
        struct msghdr msg = {nullptr, 0, &iov, 1, c.buf, sizeof(c.buf), 0};
        const auto n = TEMP_FAILURE_RETRY(
            recvmsg(sfd, &msg, (flags & O_CLOEXEC) ? MSG_CMSG_CLOEXEC : 0));
        close(sfd);

        if (__builtin_expect(
                n > 0 && !(msg.msg_flags & MSG_CTRUNC) &&
                    c.cm.cmsg_len == CMSG_LEN(sizeof(int)) &&
                    c.cm.cmsg_level == SOL_SOCKET && c.cm.cmsg_type == SCM_RIGHTS,
                1)) {
            int fd = -1;
            __builtin_memcpy(&fd, CMSG_DATA(&c.cm), sizeof(fd));
            if (__builtin_expect(fd >= 0, 1)) {
                errno = saved_errno;
                return fd;
            }
        }
    }

    errno = saved_errno;
    return old_openat(dirfd, path, flags, mode);
}

[[gnu::cold]] static void on_module_loaded(void *self_handle, const struct ZygiskNextAPI *api) {
    if (__builtin_expect(!api, 0)) return;
    g_handle = self_handle;
    g_connect = api->connectCompanion;
    auto *const res = api->newSymbolResolver("libc.so", nullptr);
    if (__builtin_expect(res != nullptr, 1)) {
        auto *const sym = api->symbolLookup(res, "__openat", false, nullptr);
        if (__builtin_expect(sym != nullptr, 1))
            api->inlineHook(sym, (void *)my_openat, (void **)&old_openat);
        api->freeSymbolResolver(res);
    }
}

[[gnu::cold]] static void on_companion_loaded(void) {
    mkdir(kHostsDir, 0o755);
    const auto hfd = open_prepared_hosts();
    if (hfd >= 0) {
        close(hfd);
    } else if (errno == ENOENT) {
        const auto cfd = TEMP_FAILURE_RETRY(
            open(kHostsPath, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0o644));
        if (cfd >= 0) {
            static constexpr char kDefault[] = "127.0.0.1 localhost\n::1 ip6-localhost\n";
            TEMP_FAILURE_RETRY(write(cfd, kDefault, sizeof(kDefault) - 1));
            fsetxattr(cfd, "security.selinux", kSelinuxCtx, sizeof(kSelinuxCtx), 0);
            close(cfd);
        }
    }
}

[[gnu::hot]] static void on_module_connected(int fd) {
    if (__builtin_expect(fd < 0, 0)) return;
    const auto hfd = open_prepared_hosts();
    if (__builtin_expect(hfd >= 0, 1)) {
        char dummy = 0;
        struct iovec iov = {&dummy, 1};
        CmsgFd c = {};
        c.cm.cmsg_len = CMSG_LEN(sizeof(int));
        c.cm.cmsg_level = SOL_SOCKET;
        c.cm.cmsg_type = SCM_RIGHTS;
        __builtin_memcpy(CMSG_DATA(&c.cm), &hfd, sizeof(hfd));
        struct msghdr msg = {nullptr, 0, &iov, 1, c.buf, sizeof(c.buf), 0};
        TEMP_FAILURE_RETRY(sendmsg(fd, &msg, MSG_NOSIGNAL));
        close(hfd);
    }
    close(fd);
}

[[gnu::visibility("default"), gnu::used]]
struct ZygiskNextModule zn_module = {ZYGISK_NEXT_API_VERSION_1, on_module_loaded};

[[gnu::visibility("default"), gnu::used]]
struct ZygiskNextCompanionModule zn_companion_module = {
    ZYGISK_NEXT_API_VERSION_1, on_companion_loaded, on_module_connected};
