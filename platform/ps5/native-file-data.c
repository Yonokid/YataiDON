#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

extern ssize_t sceKernelRead(int, void*, size_t);
extern int sceKernelClose(int);

static int kernel_errno(int64_t result) {
    uint32_t code = (uint32_t)result;
    return (code & 0xffff0000u) == 0x80020000u ? (int)(code & 0xffffu) : EIO;
}

// raylib frees callback buffers with free(). Read into an uninitialised heap
// allocation, without stdio's two seeks, zero-fill or intermediate buffer.
unsigned char* ps5_load_file_data(const char* path, int* size) {
    if (!size) { errno = EINVAL; return NULL; }
    *size = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat info;
    if (fstat(fd, &info) < 0) {
        int saved = errno; sceKernelClose(fd); errno = saved; return NULL;
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0 || info.st_size > INT_MAX) {
        sceKernelClose(fd); errno = EFBIG; return NULL;
    }
    size_t length = (size_t)info.st_size;
    unsigned char* data = malloc(length + 1);
    if (!data) { sceKernelClose(fd); errno = ENOMEM; return NULL; }
    size_t done = 0;
    while (done < length) {
        size_t count = length - done;
        if (count > (1u << 20)) count = 1u << 20;
        ssize_t read = sceKernelRead(fd, data + done, count);
        if (read < 0 && kernel_errno(read) == EINTR) continue;
        if (read <= 0 || (size_t)read > count) {
            int saved = read < 0 ? kernel_errno(read) : EIO;
            free(data); sceKernelClose(fd); errno = saved; return NULL;
        }
        done += (size_t)read;
    }
    data[length] = 0;
    sceKernelClose(fd);
    *size = (int)length;
    return data;
}
