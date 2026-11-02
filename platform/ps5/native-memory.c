// Native titles must enter libkernel rather than issue payload syscalls.
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

extern int sceKernelMmap(void*, size_t, int, int, int, off_t, void**);
extern int sceKernelMunmap(void*, size_t);
extern int sceKernelMprotect(const void*, size_t, int);

static int posix_result(int result) {
    if (result == 0) return 0;
    uint32_t code = (uint32_t)result;
    errno = (code & 0xffff0000u) == 0x80020000u
        ? (int)(code & 0xffffu)
        : (result < 0 && result >= -4095 ? -result : EIO);
    return -1;
}

void* __wrap_mmap(void* address, size_t length, int protection, int flags,
                  int descriptor, off_t offset) {
    void* mapped = MAP_FAILED;
    if (posix_result(sceKernelMmap(address, length, protection, flags,
                                 descriptor, offset, &mapped)) < 0)
        return MAP_FAILED;
    return mapped;
}

int __wrap_munmap(void* address, size_t length) {
    return posix_result(sceKernelMunmap(address, length));
}

int __wrap_mprotect(void* address, size_t length, int protection) {
    return posix_result(sceKernelMprotect(address, length, protection));
}
