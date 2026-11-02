#include <errno.h>
#include <reent.h>
#include <stddef.h>
#include <stdint.h>
#include <switch.h>

extern char* fake_heap_start;
extern char* fake_heap_end;

static uintptr_t heap_base;
static size_t heap_size;
static size_t heap_limit;
static size_t loader_heap_size;
static char* heap_break;
static Mutex heap_mutex;

static const size_t heap_alignment = 2 * 1024 * 1024;
static const size_t heap_slack = 8 * 1024 * 1024;

static size_t align_heap(size_t size) {
    return (size + heap_alignment - 1) & ~(heap_alignment - 1);
}

static bool resize_heap(size_t size) {
    void* address = NULL;
    if (R_FAILED(svcSetHeapSize(&address, size))) {
        return false;
    }
    if ((uintptr_t)address != heap_base) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
    }
    heap_size = size;
    fake_heap_end = (char*)(heap_base + size);
    return true;
}

void __libnx_initheap(void) {
    u64 base = 0;
    if (R_FAILED(svcGetInfo(&base, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0))) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
    }
    heap_base = base;

    if (envHasHeapOverride()) {
        fake_heap_start = envGetHeapOverrideAddr();
        fake_heap_end = fake_heap_start + envGetHeapOverrideSize();
        MemoryInfo info;
        u32 page_info = 0;
        // Only resize a loader override backed by the kernel heap; other loaders may supply static memory.
        if (R_FAILED(svcQueryMemory(&info, &page_info, (uintptr_t)fake_heap_start)) || info.type != MemType_Heap ||
            info.addr < heap_base || info.addr + info.size != (uintptr_t)fake_heap_end) {
            return;
        }
        heap_limit = (uintptr_t)fake_heap_end - heap_base;
        heap_size = loader_heap_size = heap_limit;
    } else {
        u64 available = 0, used = 0;
        if (R_FAILED(svcGetInfo(&available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0)) ||
            R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)) ||
            available <= used + heap_alignment) {
            diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
        }
        heap_limit = (available - used - heap_alignment) & ~(heap_alignment - 1);
        fake_heap_start = (char*)heap_base;
    }

    // Preserve the NRO's mapped code prefix and argv space, releasing only the unused tail.
    size_t initial = align_heap((uintptr_t)fake_heap_start - heap_base + 32 * 1024 * 1024);
    if (initial > heap_limit) {
        initial = heap_limit;
    }
    if (!resize_heap(initial) && !heap_size) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
    }
}

void* _sbrk_r(struct _reent* reent, ptrdiff_t increment) {
    mutexLock(&heap_mutex);
    if (!heap_break) {
        // argvSetup advances fake_heap_start after __libnx_initheap.
        heap_break = fake_heap_start;
    }
    uintptr_t previous = (uintptr_t)heap_break;
    uintptr_t limit = heap_limit ? heap_base + heap_limit : (uintptr_t)fake_heap_end;
    uintptr_t next = previous;
    bool valid;
    if (increment >= 0) {
        valid = (uintptr_t)increment <= limit - previous;
        if (valid) {
            next += (uintptr_t)increment;
        }
    } else {
        uintptr_t decrease = (uintptr_t)(-(increment + 1)) + 1;
        valid = decrease <= previous - (uintptr_t)fake_heap_start;
        if (valid) {
            next -= decrease;
        }
    }
    if (valid && heap_limit) {
        size_t required = next - heap_base;
        size_t target = align_heap(required + heap_slack);
        if (target > heap_limit) {
            target = heap_limit;
        }
        if (required > heap_size) {
            valid = resize_heap(target) || resize_heap(align_heap(required));
        } else if (increment < 0 && heap_size - target >= 2 * heap_slack && target < heap_size) {
            resize_heap(target);
        }
    }
    if (valid) {
        heap_break = (char*)next;
    } else {
        reent->_errno = ENOMEM;
    }
    mutexUnlock(&heap_mutex);
    return valid ? (void*)previous : (void*)-1;
}

NX_NORETURN void __real___nx_exit(Result rc, LoaderReturnFn return_address);

NX_NORETURN void __wrap___nx_exit(Result rc, LoaderReturnFn return_address) {
    // hbloader reuses its original heap bounds when loading hbmenu or a chained NRO.
    if (loader_heap_size && heap_size != loader_heap_size && !resize_heap(loader_heap_size)) {
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
    }
    __real___nx_exit(rc, return_address);
}
