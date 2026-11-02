// Native title stat calls: use absolute paths and translate SCE errors explicitly.
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern int sceKernelStat(const char*, struct stat*);
extern int sceKernelFstat(int, struct stat*);

static int posix_result(int result) {
    if (result >= 0) return result;
    uint32_t code = (uint32_t)result;
    if ((code & 0xffff0000u) == 0x80020000u)
        errno = (int)(code & 0xffffu);
    else
        errno = result >= -4095 ? -result : EIO;
    return -1;
}

static const char* absolute_path(const char* path, char buffer[PATH_MAX]) {
    if (!path) { errno = EINVAL; return NULL; }
    if (!path[0]) { errno = ENOENT; return NULL; }
    if (path[0] == '/') return path;
    int length = snprintf(buffer, PATH_MAX, "/data/YataiDON/%s", path);
    if (length < 0 || length >= PATH_MAX) { errno = ENAMETOOLONG; return NULL; }
    return buffer;
}

int __wrap_stat(const char* path, struct stat* info) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    if (!absolute) return -1;
    return posix_result(sceKernelStat(absolute, info));
}

int __wrap_fstat(int descriptor, struct stat* info) {
    return posix_result(sceKernelFstat(descriptor, info));
}

int __wrap_lstat(const char* path, struct stat* info) {
    // Native title storage has no supported Unix symlink interface. Use its
    // normal metadata query instead of the unsupported lstat syscall path.
    return __wrap_stat(path, info);
}

#ifndef YATAIDON_FS_STAT_TEST_ONLY
// Native application chdir is denied. Give the statically linked game a
// consistent logical working directory at every pathname-based I/O boundary.
extern FILE* __real_fopen(const char*, const char*);
extern FILE* __real_freopen(const char*, const char*, FILE*);
extern int __real_open(const char*, int, ...);
extern DIR* ps5_native_opendir(const char*);
extern int __real_mkdir(const char*, mode_t);
extern int __real_unlink(const char*);
extern int __real_rmdir(const char*);
extern int __real_remove(const char*);
extern int __real_rename(const char*, const char*);
extern int __real_access(const char*, int);
extern char* __real_realpath(const char*, char*);

long __wrap_pathconf(const char* path, int name) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    if (!absolute) return -1;
    struct stat info;
    if (__wrap_stat(absolute, &info) < 0) return -1;
    // libc++ queries this before getcwd(). Native titles cannot issue the
    // payload libc's pathconf syscall; use the SDK's compile-time limits.
    if (name == _PC_PATH_MAX) return PATH_MAX;
    if (name == _PC_NAME_MAX) return NAME_MAX;
    errno = EINVAL;
    return -1;
}

FILE* __wrap_fopen(const char* path, const char* mode) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    return absolute ? __real_fopen(absolute, mode) : NULL;
}
FILE* __wrap_freopen(const char* path, const char* mode, FILE* stream) {
    if (!path) return __real_freopen(NULL, mode, stream);
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    return absolute ? __real_freopen(absolute, mode, stream) : NULL;
}
int __wrap_open(const char* path, int flags, ...) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    if (!absolute) return -1;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode_t mode = (mode_t)va_arg(arguments, int);
        va_end(arguments);
        return __real_open(absolute, flags, mode);
    }
    return __real_open(absolute, flags);
}
DIR* __wrap_opendir(const char* path) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    return absolute ? ps5_native_opendir(absolute) : NULL;
}
int __wrap_mkdir(const char* path, mode_t mode) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    return absolute ? __real_mkdir(absolute, mode) : -1;
}
#define WRAP_PATH_CALL(name) \
    int __wrap_##name(const char* path) { \
        char buffer[PATH_MAX]; \
        const char* absolute = absolute_path(path, buffer); \
        return absolute ? __real_##name(absolute) : -1; \
    }
WRAP_PATH_CALL(unlink)
WRAP_PATH_CALL(rmdir)
WRAP_PATH_CALL(remove)

int __wrap_rename(const char* from, const char* to) {
    char first[PATH_MAX], second[PATH_MAX];
    const char* source = absolute_path(from, first);
    if (!source) return -1;
    const char* destination = absolute_path(to, second);
    return destination ? __real_rename(source, destination) : -1;
}
int __wrap_access(const char* path, int mode) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    return absolute ? __real_access(absolute, mode) : -1;
}
char* __wrap_realpath(const char* path, char* output) {
    char buffer[PATH_MAX];
    const char* absolute = absolute_path(path, buffer);
    if (!absolute) return NULL;
    // Native storage uses the same no-symlink policy as __wrap_lstat. Avoid
    // libc realpath's unsupported readlink/openat path and normalize locally.
    char normalized[PATH_MAX];
    size_t length = 1;
    normalized[0] = '/';
    const char* cursor = absolute;
    while (*cursor) {
        while (*cursor == '/') ++cursor;
        const char* start = cursor;
        while (*cursor && *cursor != '/') ++cursor;
        size_t count = (size_t)(cursor - start);
        if (!count || (count == 1 && start[0] == '.')) continue;
        if (count == 2 && start[0] == '.' && start[1] == '.') {
            while (length > 1 && normalized[length - 1] != '/') --length;
            if (length > 1) --length;
            continue;
        }
        size_t separator = length > 1;
        if (length + separator + count >= sizeof(normalized)) { errno = ENAMETOOLONG; return NULL; }
        if (separator) normalized[length++] = '/';
        memcpy(normalized + length, start, count);
        length += count;
    }
    normalized[length] = 0;
    struct stat info;
    if (__wrap_stat(normalized, &info) < 0) return NULL;
    if (!output) {
        output = malloc(length + 1);
        if (!output) { errno = ENOMEM; return NULL; }
    }
    memcpy(output, normalized, length + 1);
    return output;
}
char* __wrap_getcwd(char* output, size_t size) {
    static const char root[] = "/data/YataiDON";
    if (!output) {
        if (!size) size = sizeof(root);
        if (size < sizeof(root)) { errno = ERANGE; return NULL; }
        output = malloc(size);
        if (!output) return NULL;
    } else if (size < sizeof(root)) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(output, root, sizeof(root));
    return output;
}
int __wrap_chdir(const char* path) {
    if (path && (!strcmp(path, "/data/YataiDON") || !strcmp(path, "/data/YataiDON/") || !strcmp(path, ".")))
        return 0;
    errno = ENOTSUP;
    return -1;
}
#endif
