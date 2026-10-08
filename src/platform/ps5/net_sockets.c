// ProsperoLichess - Sockets and name lookups for libcurl on the console.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// libcurl is a PacBrew build made for the payload SDK's C library. Two things
// it relies on do not work as is in a native title:
//  - getaddrinfo and friends are imported from a system module a native
//    title does not load, so the calls would jump to address 0 (ProsperoRadio,
//    2026-10-02). They are defined here on the system resolver.
//  - In the title's sandbox, the C library's sockets did not connect in
//    ProsperoRadio's test, while sockets made with the system's sceNet calls
//    work in a sandboxed title (ProsperoLight). curl's sockets are therefore
//    opened with sceNetSocket (pch_net_open, from CURLOPT_OPENSOCKETFUNCTION)
//    and the linker routes curl's socket calls through the wrappers below
//    (--wrap, see APP_WRAP_SYMBOLS in the Makefile): a socket opened here goes
//    to its sceNet counterpart, any other descriptor to the C library.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdarg.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

struct sce_net_epoll_event
{
    uint32_t events;
    uint32_t pad;
    uint64_t ident;
    union
    {
        void *pointer;
        uint32_t value;
        uint64_t value64;
        int socket;
    } data;
};

_Static_assert(sizeof(struct sce_net_epoll_event) == 24, "sceNet epoll event ABI mismatch");

enum
{
    PCH_EPOLLIN = 0x01,
    PCH_EPOLLOUT = 0x02,
    PCH_EPOLLERR = 0x08,
    PCH_EPOLLHUP = 0x10,
    PCH_EPOLL_CTL_ADD = 1,
    PCH_SO_NBIO = 0x1200,
};

extern int sceKernelUsleep(uint32_t microseconds);
extern int sceNetSocket(const char *name, int domain, int type, int protocol);
extern int sceNetSocketClose(int socket);
extern int sceNetConnect(int socket, const struct sockaddr *address, socklen_t length);
extern int sceNetSend(int socket, const void *buffer, size_t length, int flags);
extern int sceNetRecv(int socket, void *buffer, size_t length, int flags);
extern int sceNetSetsockopt(int socket, int level, int option, const void *value, socklen_t length);
extern int sceNetGetsockopt(int socket, int level, int option, void *value, socklen_t *length);
extern int sceNetGetsockname(int socket, struct sockaddr *address, socklen_t *length);
extern int sceNetGetpeername(int socket, struct sockaddr *address, socklen_t *length);
extern int sceNetShutdown(int socket, int how);
extern int *sceNetErrnoLoc(void);
extern int sceNetEpollCreate(const char *name, int flags);
extern int sceNetEpollDestroy(int epoll);
extern int sceNetEpollControl(int epoll, int operation, int socket,
                              struct sce_net_epoll_event *event);
extern int sceNetEpollWait(int epoll, struct sce_net_epoll_event *events, int maximum,
                           int timeout_us);
extern int sceNetPoolCreate(const char *name, int size, int flags);
extern int sceNetPoolDestroy(int pool);
extern int sceNetResolverCreate(const char *name, int pool, int flags);
extern int sceNetResolverStartNtoa(int resolver, const char *hostname, struct in_addr *address,
                                   int timeout_us, int retries, int flags);
extern int sceNetResolverDestroy(int resolver);

extern int __real_connect(int socket, const struct sockaddr *address, socklen_t length);
extern ssize_t __real_send(int socket, const void *buffer, size_t length, int flags);
extern ssize_t __real_recv(int socket, void *buffer, size_t length, int flags);
extern int __real_setsockopt(int socket, int level, int option, const void *value,
                             socklen_t length);
extern int __real_getsockopt(int socket, int level, int option, void *value, socklen_t *length);
extern int __real_getsockname(int socket, struct sockaddr *address, socklen_t *length);
extern int __real_getpeername(int socket, struct sockaddr *address, socklen_t *length);
extern int __real_shutdown(int socket, int how);
extern int __real_fcntl(int descriptor, int command, ...);
extern int __real_poll(struct pollfd *descriptors, nfds_t count, int timeout_ms);

// ---- the sockets opened here ----

#define PCH_MAX_SOCKETS 64
static _Atomic int g_sockets[PCH_MAX_SOCKETS];

static int ours(int socket)
{
    if (socket < 0)
        return 0;
    for (int i = 0; i < PCH_MAX_SOCKETS; ++i)
    {
        if (atomic_load(&g_sockets[i]) == socket + 1)
            return 1;
    }
    return 0;
}

static int result_of(int result)
{
    if (result < 0)
    {
        const int *error = sceNetErrnoLoc();
        errno = error != NULL ? *error : EIO;
        return -1;
    }
    return result;
}

int pch_net_open(int domain, int type, int protocol)
{
    const int socket = result_of(sceNetSocket("pch_curl", domain, type, protocol));
    if (socket < 0)
        return -1;
    // The C library refuses O_NONBLOCK on sockets: the system's own option.
    const int on = 1;
    sceNetSetsockopt(socket, SOL_SOCKET, PCH_SO_NBIO, &on, sizeof(on));
    for (int i = 0; i < PCH_MAX_SOCKETS; ++i)
    {
        int empty = 0;
        if (atomic_compare_exchange_strong(&g_sockets[i], &empty, socket + 1))
            return socket;
    }
    // No room to remember it: it would be served by the C library, so refuse.
    sceNetSocketClose(socket);
    errno = EMFILE;
    return -1;
}

int pch_net_close(int socket)
{
    for (int i = 0; i < PCH_MAX_SOCKETS; ++i)
    {
        int expected = socket + 1;
        if (atomic_compare_exchange_strong(&g_sockets[i], &expected, 0))
            return result_of(sceNetSocketClose(socket));
    }
    errno = EBADF;
    return -1;
}

int __wrap_connect(int socket, const struct sockaddr *address, socklen_t length)
{
    if (!ours(socket))
        return __real_connect(socket, address, length);
    return result_of(sceNetConnect(socket, address, length));
}

ssize_t __wrap_send(int socket, const void *buffer, size_t length, int flags)
{
    if (!ours(socket))
        return __real_send(socket, buffer, length, flags);
    return result_of(sceNetSend(socket, buffer, length, flags & ~MSG_NOSIGNAL));
}

ssize_t __wrap_recv(int socket, void *buffer, size_t length, int flags)
{
    if (!ours(socket))
        return __real_recv(socket, buffer, length, flags);
    return result_of(sceNetRecv(socket, buffer, length, flags & ~MSG_NOSIGNAL));
}

int __wrap_setsockopt(int socket, int level, int option, const void *value, socklen_t length)
{
    if (!ours(socket))
        return __real_setsockopt(socket, level, option, value, length);
    return result_of(sceNetSetsockopt(socket, level, option, value, length));
}

int __wrap_getsockopt(int socket, int level, int option, void *value, socklen_t *length)
{
    if (!ours(socket))
        return __real_getsockopt(socket, level, option, value, length);
    return result_of(sceNetGetsockopt(socket, level, option, value, length));
}

int __wrap_getsockname(int socket, struct sockaddr *address, socklen_t *length)
{
    if (!ours(socket))
        return __real_getsockname(socket, address, length);
    return result_of(sceNetGetsockname(socket, address, length));
}

int __wrap_getpeername(int socket, struct sockaddr *address, socklen_t *length)
{
    if (!ours(socket))
        return __real_getpeername(socket, address, length);
    return result_of(sceNetGetpeername(socket, address, length));
}

int __wrap_shutdown(int socket, int how)
{
    if (!ours(socket))
        return __real_shutdown(socket, how);
    return result_of(sceNetShutdown(socket, how));
}

// curl marks every socket close-on-exec and non-blocking with fcntl, which the
// C library refuses on sockets (EINVAL). A title never execs, so close-on-exec
// is accepted as is; non-blocking goes through the system's own option.
static int socket_flags(int socket, int command, int value,
                        int (*set)(int, int, int, const void *, socklen_t),
                        int (*get)(int, int, int, void *, socklen_t *))
{
    switch (command)
    {
    case F_GETFD:
    case F_SETFD:
        return 0;
    case F_SETFL:
    {
        const int on = (value & O_NONBLOCK) != 0;
        return set(socket, SOL_SOCKET, PCH_SO_NBIO, &on, sizeof(on));
    }
    case F_GETFL:
    {
        int on = 0;
        socklen_t length = sizeof(on);
        if (get(socket, SOL_SOCKET, PCH_SO_NBIO, &on, &length) < 0)
            return -1;
        return O_RDWR | (on ? O_NONBLOCK : 0);
    }
    default:
        errno = EINVAL;
        return -1;
    }
}

static int sce_set(int socket, int level, int option, const void *value, socklen_t length)
{
    return result_of(sceNetSetsockopt(socket, level, option, value, length));
}

static int sce_get(int socket, int level, int option, void *value, socklen_t *length)
{
    return result_of(sceNetGetsockopt(socket, level, option, value, length));
}

int __wrap_fcntl(int descriptor, int command, ...)
{
    va_list arguments;
    va_start(arguments, command);
    // Every command curl or the libraries use takes an int or a pointer; the
    // pointer-sized read keeps both intact for the C library.
    const intptr_t argument = va_arg(arguments, intptr_t);
    va_end(arguments);
    if (ours(descriptor))
        return socket_flags(descriptor, command, (int)argument, sce_set, sce_get);
    const int result = __real_fcntl(descriptor, command, argument);
    if (result >= 0 || errno != EINVAL)
        return result;
    // A socket from the C library (curl's libc mode): same answers, its options.
    if (command == F_GETFD || command == F_SETFD || command == F_SETFL || command == F_GETFL)
        return socket_flags(descriptor, command, (int)argument, __real_setsockopt,
                            __real_getsockopt);
    return result;
}

// Our sockets are waited on with the system's epoll; anything else in the
// same call (curl's wake-up pipe) is checked with the C library's poll.
// Mixed sets are served in short slices of both.
static int poll_ours(struct pollfd *descriptors, nfds_t count, int timeout_us)
{
    struct sce_net_epoll_event events[16];
    const int epoll = result_of(sceNetEpollCreate("pch_curl_poll", 0));
    if (epoll < 0)
        return -1;
    int added = 0;
    for (nfds_t i = 0; i < count && added < 16; ++i)
    {
        if (!ours(descriptors[i].fd))
            continue;
        struct sce_net_epoll_event event;
        memset(&event, 0, sizeof(event));
        if ((descriptors[i].events & (POLLIN | POLLRDNORM)) != 0)
            event.events |= PCH_EPOLLIN;
        if ((descriptors[i].events & (POLLOUT | POLLWRNORM)) != 0)
            event.events |= PCH_EPOLLOUT;
        event.data.value = (uint32_t)i;
        if (sceNetEpollControl(epoll, PCH_EPOLL_CTL_ADD, descriptors[i].fd, &event) >= 0)
            ++added;
    }
    int ready = 0;
    if (added > 0)
    {
        ready = result_of(sceNetEpollWait(epoll, events, added, timeout_us));
        for (int i = 0; i < ready; ++i)
        {
            const uint32_t index = events[i].data.value;
            if (index >= count)
                continue;
            short revents = 0;
            if ((events[i].events & PCH_EPOLLIN) != 0)
                revents |= POLLIN;
            if ((events[i].events & PCH_EPOLLOUT) != 0)
                revents |= POLLOUT;
            if ((events[i].events & PCH_EPOLLERR) != 0)
                revents |= POLLERR;
            if ((events[i].events & PCH_EPOLLHUP) != 0)
                revents |= POLLHUP;
            descriptors[index].revents |= revents;
        }
    }
    else if (timeout_us > 0)
    {
        sceKernelUsleep((uint32_t)timeout_us);
    }
    sceNetEpollDestroy(epoll);
    return ready;
}

int __wrap_poll(struct pollfd *descriptors, nfds_t count, int timeout_ms)
{
    int mine = 0;
    for (nfds_t i = 0; i < count; ++i)
        mine += ours(descriptors[i].fd);
    if (mine == 0)
        return __real_poll(descriptors, count, timeout_ms);

    struct pollfd others[16];
    nfds_t other_index[16];
    nfds_t other_count = 0;
    for (nfds_t i = 0; i < count; ++i)
    {
        descriptors[i].revents = 0;
        if (!ours(descriptors[i].fd) && descriptors[i].fd >= 0 && other_count < 16)
        {
            others[other_count] = descriptors[i];
            other_index[other_count] = i;
            ++other_count;
        }
    }
    // Wait in slices of at most 10 ms so the other descriptors are seen too.
    int left_ms = timeout_ms < 0 ? INT_MAX : timeout_ms;
    for (;;)
    {
        const int slice_ms = other_count == 0 ? left_ms : (left_ms < 10 ? left_ms : 10);
        int ready =
            poll_ours(descriptors, count, slice_ms > INT_MAX / 1000 ? INT_MAX : slice_ms * 1000);
        if (ready < 0)
            return -1;
        if (other_count > 0 && __real_poll(others, other_count, 0) > 0)
        {
            for (nfds_t i = 0; i < other_count; ++i)
            {
                if (others[i].revents != 0)
                {
                    descriptors[other_index[i]].revents = others[i].revents;
                    ++ready;
                }
            }
        }
        if (ready > 0 || left_ms <= slice_ms)
            return ready;
        left_ms -= slice_ms;
    }
}

// ---- name lookups (IPv4, one address per name, the system's resolver) ----

static int lookup(const char *name, struct in_addr *address)
{
    if (inet_pton(AF_INET, name, address) == 1)
        return 0;
    const int pool = sceNetPoolCreate("pch_dns", 16 * 1024, 0);
    if (pool < 0)
        return EAI_MEMORY;
    int result = EAI_FAIL;
    const int resolver = sceNetResolverCreate("pch_dns", pool, 0);
    if (resolver >= 0)
    {
        result =
            sceNetResolverStartNtoa(resolver, name, address, 5000000, 2, 0) < 0 ? EAI_NONAME : 0;
        sceNetResolverDestroy(resolver);
    }
    sceNetPoolDestroy(pool);
    return result;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
                struct addrinfo **result)
{
    *result = NULL;
    const int family = hints != NULL ? hints->ai_family : AF_UNSPEC;
    if (family != AF_UNSPEC && family != AF_INET)
        return EAI_FAMILY;
    struct in_addr address;
    address.s_addr = htonl(INADDR_LOOPBACK);
    if (node != NULL)
    {
        if (hints != NULL && (hints->ai_flags & AI_NUMERICHOST) != 0)
        {
            if (inet_pton(AF_INET, node, &address) != 1)
                return EAI_NONAME;
        }
        else
        {
            const int failed = lookup(node, &address);
            if (failed != 0)
                return failed;
        }
    }
    else if (hints != NULL && (hints->ai_flags & AI_PASSIVE) != 0)
    {
        address.s_addr = htonl(INADDR_ANY);
    }

    // One block: the entry, then its address.
    struct addrinfo *entry = calloc(1, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    if (entry == NULL)
        return EAI_MEMORY;
    struct sockaddr_in *in = (struct sockaddr_in *)(entry + 1);
    in->sin_len = sizeof(*in);
    in->sin_family = AF_INET;
    in->sin_port = htons((uint16_t)(service != NULL ? atoi(service) : 0));
    in->sin_addr = address;
    entry->ai_family = AF_INET;
    entry->ai_socktype =
        hints != NULL && hints->ai_socktype != 0 ? hints->ai_socktype : SOCK_STREAM;
    entry->ai_protocol = hints != NULL ? hints->ai_protocol : 0;
    entry->ai_addrlen = sizeof(*in);
    entry->ai_addr = (struct sockaddr *)in;
    *result = entry;
    return 0;
}

void freeaddrinfo(struct addrinfo *entry)
{
    while (entry != NULL)
    {
        struct addrinfo *next = entry->ai_next;
        free(entry);
        entry = next;
    }
}

const char *gai_strerror(int code)
{
    switch (code)
    {
    case 0:
        return "no error";
    case EAI_NONAME:
        return "the name was not found";
    case EAI_FAMILY:
        return "address family not supported";
    case EAI_MEMORY:
        return "out of memory";
    default:
        return "the name lookup failed";
    }
}

struct hostent *gethostbyname(const char *name)
{
    (void)name;
    return NULL; // libcurl uses getaddrinfo
}

int getnameinfo(const struct sockaddr *address, socklen_t length, char *host, size_t host_size,
                char *service, size_t service_size, int flags)
{
    (void)flags;
    if (address == NULL || address->sa_family != AF_INET || length < sizeof(struct sockaddr_in))
        return EAI_FAMILY;
    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
    if (host != NULL && host_size != 0 &&
        inet_ntop(AF_INET, &in->sin_addr, host, (socklen_t)host_size) == NULL)
        return EAI_FAIL;
    if (service != NULL && service_size != 0)
        snprintf(service, service_size, "%u", (unsigned)ntohs(in->sin_port));
    return 0;
}

// '*' and '?' only, which is all libcurl could ask for.
int fnmatch(const char *pattern, const char *text, int flags)
{
    (void)flags;
    for (; *pattern != '\0'; ++pattern, ++text)
    {
        if (*pattern == '*')
        {
            while (*pattern == '*')
                ++pattern;
            if (*pattern == '\0')
                return 0;
            for (; *text != '\0'; ++text)
            {
                if (fnmatch(pattern, text, flags) == 0)
                    return 0;
            }
            return FNM_NOMATCH;
        }
        if (*text == '\0' || (*pattern != '?' && *pattern != *text))
            return FNM_NOMATCH;
    }
    return *text == '\0' ? 0 : FNM_NOMATCH;
}
