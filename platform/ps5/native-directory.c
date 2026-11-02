#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define DIRECTORY_BUFFER_SIZE 65536

extern int sceKernelOpen(const char*, int, int);
extern int sceKernelClose(int);
extern int sceKernelGetdents(int, char*, int);

typedef struct {
    int fd;
    size_t position, length;
    struct dirent entry;
    char* buffer;
} NativeDirectory;

static int posix_result(int result) {
    if (result >= 0) return result;
    uint32_t code = (uint32_t)result;
    errno = (code & 0xffff0000u) == 0x80020000u
        ? (int)(code & 0xffffu) : EIO;
    return -1;
}

DIR* __wrap_fdopendir(int fd) {
    NativeDirectory* directory = calloc(1, sizeof(*directory));
    if (!directory) { errno = ENOMEM; return NULL; }
    // PS5 directory reads require a page-aligned buffer. A flexible mapping
    // also keeps the kernel I/O buffer separate from the direct-memory heap.
    directory->buffer = mmap(NULL, DIRECTORY_BUFFER_SIZE, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (directory->buffer == MAP_FAILED) {
        int saved = errno;
        free(directory);
        errno = saved;
        return NULL;
    }
    directory->fd = fd;
    return (DIR*)directory;
}

DIR* ps5_native_opendir(const char* path) {
    // Native libkernel does not accept all payload libc open flags.
    int raw = sceKernelOpen(path, O_RDONLY, 0);
    int fd = posix_result(raw);
    if (fd < 0) fprintf(stderr, "[yataidon-ps5] directory open failed path=%s result=%08x errno=%d\n", path, (unsigned)raw, errno);
    if (fd < 0) return NULL;
    DIR* directory = __wrap_fdopendir(fd);
    if (!directory) {
        int saved = errno;
        sceKernelClose(fd);
        errno = saved;
    }
    return directory;
}

struct dirent* __wrap_readdir(DIR* stream) {
    NativeDirectory* directory = (NativeDirectory*)stream;
    if (!directory) { errno = EBADF; return NULL; }
    for (;;) {
        if (directory->position == directory->length) {
            int raw = sceKernelGetdents(directory->fd,
                directory->buffer, DIRECTORY_BUFFER_SIZE);
            int count = posix_result(raw);
            if (count < 0) fprintf(stderr, "[yataidon-ps5] directory read failed fd=%d result=%08x errno=%d\n", directory->fd, (unsigned)raw, errno);
            if (count <= 0) return NULL;
            if (count > DIRECTORY_BUFFER_SIZE) { errno = EIO; return NULL; }
            directory->position = 0;
            directory->length = (size_t)count;
        }
        size_t remaining = directory->length - directory->position;
        const struct dirent* entry = (const struct dirent*)(directory->buffer + directory->position);
        size_t header = offsetof(struct dirent, d_name);
        if (remaining < header || entry->d_reclen > remaining ||
            entry->d_reclen < header + 1) { errno = EIO; return NULL; }
        size_t name_bytes = entry->d_reclen - header;
        if (name_bytes > sizeof(entry->d_name)) name_bytes = sizeof(entry->d_name);
        if (!memchr(entry->d_name, 0, name_bytes)) { errno = EIO; return NULL; }
        memset(&directory->entry, 0, sizeof(directory->entry));
        size_t copy = entry->d_reclen;
        if (copy > sizeof(directory->entry)) copy = sizeof(directory->entry);
        memcpy(&directory->entry, entry, copy);
        directory->position += entry->d_reclen;
        if (directory->entry.d_ino) return &directory->entry;
    }
}

int __wrap_closedir(DIR* stream) {
    NativeDirectory* directory = (NativeDirectory*)stream;
    if (!directory) { errno = EBADF; return -1; }
    int result = posix_result(sceKernelClose(directory->fd));
    int saved = errno;
    munmap(directory->buffer, DIRECTORY_BUFFER_SIZE);
    free(directory);
    errno = saved;
    return result;
}

int __wrap_dirfd(DIR* stream) {
    if (!stream) { errno = EBADF; return -1; }
    return ((NativeDirectory*)stream)->fd;
}
