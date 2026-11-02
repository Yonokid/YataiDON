#include "sqlite-vfs.h"

#include <errno.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static sqlite3_vfs sdmc_vfs;
static sqlite3_vfs* unix_vfs;
static int (*original_open)(const char*, int, int);
static int (*original_close)(int);
static int (*original_stat)(const char*, struct stat*);
static int (*original_fstat)(int, struct stat*);
static pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct OpenFile {
    int fd;
    struct OpenFile* next;
    char path[];
} OpenFile;
static OpenFile* open_files;
static ssize_t (*original_read)(int, void*, size_t);
static ssize_t (*original_pread)(int, void*, size_t, off_t);
static ssize_t (*original_pread64)(int, void*, size_t, sqlite3_int64);

static int sdmc_open(const char* path, int flags, int mode) {
    OpenFile* file = malloc(sizeof(*file) + strlen(path) + 1);
    if (!file) {
        errno = ENOMEM;
        return -1;
    }
    int fd = original_open(path, flags, mode);
    if (fd < 0) {
        free(file);
        return fd;
    }
    file->fd = fd;
    memcpy(file->path, path, strlen(path) + 1);
    pthread_mutex_lock(&files_mutex);
    file->next = open_files;
    open_files = file;
    pthread_mutex_unlock(&files_mutex);
    return fd;
}

static int sdmc_close(int fd) {
    pthread_mutex_lock(&files_mutex);
    for (OpenFile** link = &open_files; *link; link = &(*link)->next) {
        if ((*link)->fd == fd) {
            OpenFile* file = *link;
            *link = file->next;
            free(file);
            break;
        }
    }
    int result = original_close(fd);
    pthread_mutex_unlock(&files_mutex);
    return result;
}

static int sdmc_stat(const char* path, struct stat* status) {
    // libnx stat() reopens the file. Horizon can reject that while SQLite
    // already holds a writable handle, including when creating its journal.
    pthread_mutex_lock(&files_mutex);
    for (OpenFile* file = open_files; file; file = file->next) {
        if (strcmp(file->path, path) == 0) {
            int result = original_fstat(file->fd, status);
            pthread_mutex_unlock(&files_mutex);
            return result;
        }
    }
    pthread_mutex_unlock(&files_mutex);
    return original_stat(path, status);
}

static int bounded_count(int fd, sqlite3_int64 offset, size_t* count) {
    struct stat status;
    if (fstat(fd, &status) != 0) {
        return -1;
    }
    if (S_ISREG(status.st_mode) && offset >= 0) {
        sqlite3_int64 remaining = offset >= status.st_size ? 0 : status.st_size - offset;
        if ((sqlite3_uint64)remaining < *count) {
            *count = (size_t)remaining;
        }
    }
    return 0;
}

// libnx can report EIO for a request extending beyond EOF. SQLite expects a
// short read instead, including while opening a newly created empty database.
static ssize_t sdmc_read(int fd, void* buffer, size_t count) {
    off_t offset = lseek(fd, 0, SEEK_CUR);
    if (offset < 0 || bounded_count(fd, offset, &count) != 0) {
        return -1;
    }
    return count ? original_read(fd, buffer, count) : 0;
}

static ssize_t sdmc_pread(int fd, void* buffer, size_t count, off_t offset) {
    if (bounded_count(fd, offset, &count) != 0) {
        return -1;
    }
    return count ? original_pread(fd, buffer, count, offset) : 0;
}

static ssize_t sdmc_pread64(int fd, void* buffer, size_t count, sqlite3_int64 offset) {
    if (bounded_count(fd, offset, &count) != 0) {
        return -1;
    }
    return count ? original_pread64(fd, buffer, count, offset) : 0;
}

static const char* without_device(const char* path) {
    return strncmp(path, "sdmc:", 5) == 0 ? path + 5 : path;
}

static int sdmc_full_path(sqlite3_vfs* vfs, const char* name, int size, char* out) {
    (void)vfs;
    char cwd[4096];
    char absolute[4096];
    const char* path = without_device(name);
    if (strchr(path, ':')) {
        return SQLITE_CANTOPEN;
    }
    if (path[0] != '/') {
        if (!getcwd(cwd, sizeof(cwd))) {
            return SQLITE_CANTOPEN;
        }
        const char* directory = without_device(cwd);
        if (directory[0] != '/' || strchr(directory, ':')) {
            return SQLITE_CANTOPEN;
        }
        int length = snprintf(absolute, sizeof(absolute), "%s/%s", directory, path);
        if (length < 0 || (size_t)length >= sizeof(absolute)) {
            return SQLITE_CANTOPEN;
        }
        path = absolute;
    }
    // Unix's normalizer prepends '/' to every component, including 'sdmc:'.
    // Pass a device-free absolute path; the game's current device remains SD.
    return unix_vfs->xFullPathname(unix_vfs, path, size, out);
}

int switch_sqlite_initialize(void) {
    if (unix_vfs && sqlite3_vfs_find("switch-sdmc") == &sdmc_vfs) {
        return SQLITE_OK;
    }
    int result = sqlite3_initialize();
    if (result != SQLITE_OK) {
        return result;
    }
    unix_vfs = sqlite3_vfs_find("unix-dotfile");
    if (!unix_vfs) {
        return SQLITE_NOTFOUND;
    }
    original_open = (int (*)(const char*, int, int))unix_vfs->xGetSystemCall(unix_vfs, "open");
    original_close = (int (*)(int))unix_vfs->xGetSystemCall(unix_vfs, "close");
    original_stat = (int (*)(const char*, struct stat*))unix_vfs->xGetSystemCall(unix_vfs, "stat");
    original_fstat = (int (*)(int, struct stat*))unix_vfs->xGetSystemCall(unix_vfs, "fstat");
    if (!original_open || !original_close || !original_stat || !original_fstat) {
        return SQLITE_NOTFOUND;
    }
    unix_vfs->xSetSystemCall(unix_vfs, "open", (sqlite3_syscall_ptr)sdmc_open);
    unix_vfs->xSetSystemCall(unix_vfs, "close", (sqlite3_syscall_ptr)sdmc_close);
    unix_vfs->xSetSystemCall(unix_vfs, "stat", (sqlite3_syscall_ptr)sdmc_stat);
    original_read = (ssize_t (*)(int, void*, size_t))unix_vfs->xGetSystemCall(unix_vfs, "read");
    original_pread = (ssize_t (*)(int, void*, size_t, off_t))unix_vfs->xGetSystemCall(unix_vfs, "pread");
    original_pread64 = (ssize_t (*)(int, void*, size_t, sqlite3_int64))unix_vfs->xGetSystemCall(unix_vfs, "pread64");
    if (original_read) {
        unix_vfs->xSetSystemCall(unix_vfs, "read", (sqlite3_syscall_ptr)sdmc_read);
    }
    if (original_pread) {
        unix_vfs->xSetSystemCall(unix_vfs, "pread", (sqlite3_syscall_ptr)sdmc_pread);
    }
    if (original_pread64) {
        unix_vfs->xSetSystemCall(unix_vfs, "pread64", (sqlite3_syscall_ptr)sdmc_pread64);
    }
    sdmc_vfs = *unix_vfs;
    sdmc_vfs.pNext = NULL;
    sdmc_vfs.zName = "switch-sdmc";
    sdmc_vfs.xFullPathname = sdmc_full_path;
    result = sqlite3_vfs_register(&sdmc_vfs, 1);
    return result;
}
