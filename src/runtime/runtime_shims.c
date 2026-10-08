// ProsperoLichess - Process-level runtime shims for the OpenGL runtime.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Adapted from ps5-opengl native-app/runtime_shims.c: the app log receipt,
// the never-return main policy, and libc entry points the clean-room libc
// shim does not provide but the statically linked Mesa runtime references.

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

extern int sceKernelUsleep(uint32_t microseconds);
extern uint64_t sceKernelGetProcessTime(void);

#define PCH_DATA_DIR "/download0/prosperolichess"
#define PCH_LOG_PATH PCH_DATA_DIR "/app.log"

__attribute__((constructor)) static void pch_open_log(void)
{
    mkdir(PCH_DATA_DIR, 0755);
    chmod(PCH_DATA_DIR, 0755); /* earlier builds created it private */
    /* Keep the previous launch's log for post-close inspection. */
    rename(PCH_LOG_PATH, PCH_DATA_DIR "/app.prev.log");
    FILE *stream = freopen(PCH_LOG_PATH, "w", stdout);
    /* Start a fresh receipt, then make both streams append-only and unbuffered
     * so the log survives a shell close or a GPU fail-stop. */
    if (stream != NULL)
        stream = freopen(PCH_LOG_PATH, "a", stdout);
    if (stream != NULL)
        setvbuf(stream, NULL, _IONBF, 0);
    stream = freopen(PCH_LOG_PATH, "a", stderr);
    if (stream != NULL)
        setvbuf(stream, NULL, _IONBF, 0);
}

/* Returning from main or calling exit() crashes a native title; stay alive
 * until the shell closes the title. */
__attribute__((noreturn)) void catchReturnFromMain(int status)
{
    printf("[PCH] main returned status=%d\n", status);
    fflush(NULL);
    for (;;)
        sceKernelUsleep(100000);
}

void pch_glapi_tls_context_init(void) __asm__("_ZTH23_mesa_glapi_tls_Context");

void pch_glapi_tls_context_init(void)
{
}

__attribute__((noreturn)) void __assert(const char *function, const char *file, int line,
                                        const char *expression)
{
    fprintf(stderr, "[PCH] assertion failed: %s (%s:%d, %s)\n", expression, file, line, function);
    abort();
}

/* The OpenGL runtime's shader cache (PS5_SHADER_CACHE_DIR) writes through
 * mkstemp(). The SDK binds mkstemp and isatty to libScePosixForWebKit, a
 * system library a native title does not load: a call jumps to address 0.
 * These definitions are linked instead of those imports. */
int mkstemps(char *template_name, int suffix_length)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static unsigned counter;
    const size_t length = template_name ? strlen(template_name) : 0;

    if (suffix_length < 0 || length < (size_t)suffix_length + 6u)
    {
        errno = EINVAL;
        return -1;
    }
    char *name = template_name + length - (size_t)suffix_length - 6u;
    for (int i = 0; i < 6; ++i)
    {
        if (name[i] != 'X')
        {
            errno = EINVAL;
            return -1;
        }
    }
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        uint64_t value = sceKernelGetProcessTime() +
                         (uint64_t)__atomic_add_fetch(&counter, 1u, __ATOMIC_RELAXED) *
                             UINT64_C(0x9E3779B97F4A7C15);
        for (int i = 0; i < 6; ++i)
        {
            name[i] = letters[value % 36u];
            value /= 36u;
        }
        const int file = open(template_name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (file >= 0 || errno != EEXIST)
            return file;
    }
    errno = EEXIST;
    return -1;
}

int mkstemp(char *template_name)
{
    return mkstemps(template_name, 0);
}

int isatty(int descriptor)
{
    (void)descriptor;
    errno = ENOTTY;
    return 0;
}

/* The console's splash picture stays up until the app has presented its first
 * frame. The OpenGL runtime asks to hide it as soon as the display opens,
 * which leaves a black screen while programs build and fonts load. The build
 * routes every such call here (--wrap), and sys::hide_splash_screen() lets
 * them through. */
extern int __real_sceSystemServiceHideSplashScreen(void);
static int pch_splash_released;

void pch_release_splash(void)
{
    pch_splash_released = 1;
}

int __wrap_sceSystemServiceHideSplashScreen(void)
{
    return pch_splash_released ? __real_sceSystemServiceHideSplashScreen() : 0;
}

void openlog(const char *identifier, int option, int facility)
{
    (void)identifier;
    (void)option;
    (void)facility;
}

FILE *popen(const char *command, const char *mode)
{
    (void)command;
    (void)mode;
    errno = ENOSYS;
    return NULL;
}

int pclose(FILE *stream)
{
    (void)stream;
    errno = ENOSYS;
    return -1;
}

/* What libcurl and OpenSSL (PacBrew builds, made for the payload SDK's libc)
 * ask of the C library that this app's libc does not export (after
 * ProsperoRadio's curl guide, 2026-10-02). */

/* libcurl asks for the user's home folder (for .netrc): there is none. */
struct passwd;
int getpwuid_r(unsigned user, struct passwd *entry, char *buffer, size_t size,
               struct passwd **result)
{
    (void)user;
    (void)entry;
    (void)buffer;
    (void)size;
    *result = NULL;
    return ENOENT;
}

/* A plain jump keeps the caller's frame: a C wrapper would break setjmp. */
__asm__(".text\n"
        ".globl _setjmp\n"
        "_setjmp:\n"
        "    jmp *setjmp@GOTPCREL(%rip)\n"
        ".globl _longjmp\n"
        "_longjmp:\n"
        "    jmp *longjmp@GOTPCREL(%rip)\n");

void closelog(void)
{
}

int dladdr(const void *address, void *info)
{
    (void)address;
    (void)info;
    return 0;
}

unsigned if_nametoindex(const char *name)
{
    (void)name;
    return 0;
}

int pipe2(int descriptors[2], int flags)
{
    if (pipe(descriptors) != 0)
        return -1;
    for (int i = 0; i < 2; ++i)
    {
        if ((flags & O_NONBLOCK) != 0)
            fcntl(descriptors[i], F_SETFL, fcntl(descriptors[i], F_GETFL) | O_NONBLOCK);
        if ((flags & O_CLOEXEC) != 0)
            fcntl(descriptors[i], F_SETFD, FD_CLOEXEC);
    }
    return 0;
}

/* Only QUIC uses these; curl falls back when they are not there. */
int recvmmsg(int socket, void *messages, size_t count, int flags, const void *timeout)
{
    (void)socket;
    (void)messages;
    (void)count;
    (void)flags;
    (void)timeout;
    errno = ENOSYS;
    return -1;
}

int sendmmsg(int socket, void *messages, size_t count, int flags)
{
    (void)socket;
    (void)messages;
    (void)count;
    (void)flags;
    errno = ENOSYS;
    return -1;
}

/* zstd's optional tracing hooks: a begin that returns 0 means "not traced". */
unsigned long long ZSTD_trace_compress_begin(const void *context)
{
    (void)context;
    return 0;
}

void ZSTD_trace_compress_end(unsigned long long trace, const void *record)
{
    (void)trace;
    (void)record;
}

unsigned long long ZSTD_trace_decompress_begin(const void *context)
{
    (void)context;
    return 0;
}

void ZSTD_trace_decompress_end(unsigned long long trace, const void *record)
{
    (void)trace;
    (void)record;
}

/* Certificate dates are checked with this: it has to be right. Days since
 * 1970 to a civil date (Howard Hinnant's algorithm). */
struct tm *gmtime_r(const time_t *when, struct tm *out)
{
    long long seconds = (long long)*when;
    long long days = seconds / 86400;
    long long rest = seconds % 86400;
    if (rest < 0)
    {
        rest += 86400;
        --days;
    }
    memset(out, 0, sizeof(*out));
    out->tm_hour = (int)(rest / 3600);
    out->tm_min = (int)(rest % 3600 / 60);
    out->tm_sec = (int)(rest % 60);
    out->tm_wday = (int)(((days % 7) + 11) % 7);
    const long long z = days + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const long long month = mp < 10 ? mp + 3 : mp - 9;
    const long long year = yoe + era * 400 + (month <= 2 ? 1 : 0);
    out->tm_mday = (int)(doy - (153 * mp + 2) / 5 + 1);
    out->tm_mon = (int)(month - 1);
    out->tm_year = (int)(year - 1900);
    const int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    out->tm_yday = before[out->tm_mon] + out->tm_mday - 1 + (leap && out->tm_mon > 1 ? 1 : 0);
    return out;
}
