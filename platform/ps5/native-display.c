#include <stdint.h>
#include <stdio.h>

extern int __real_sceVideoOutOpen(int32_t, int32_t, int32_t, const void *);
extern int __real_sceVideoOutClose(int32_t);
extern int sceVideoOutIsOutputSupported(int32_t, uint32_t, const void *, const void *, const void *);
extern int sceVideoOutConfigureOutput(int32_t, uint32_t, const void *, const void *, const void *);
extern int sceVideoOutWaitVblank(int32_t);
static unsigned ps5_scanout_fps = 60, ps5_scanout_active_fps = 60;
static int display_handle = -1;
static int changed_live;

int __wrap_sceVideoOutOpen(int32_t user, int32_t type, int32_t index, const void *param) {
    int handle = __real_sceVideoOutOpen(user, type, index, param);
    if (handle >= 0) display_handle = handle;
    return handle;
}

/* Called on the render thread after draining GPU work; no context or assets are lost. */
int ps5_set_output_refresh(int hz) {
    if (display_handle < 0 || (hz != 60 && hz != 120)) return -1;
    const unsigned previous = ps5_scanout_active_fps;
    if (previous == (unsigned)hz) return (int)previous;
    int support = hz == 60 ? 1 : sceVideoOutIsOutputSupported(display_handle, 15, 0, 0, 0);
    if (support <= 0) return (int)previous;
    int rc = sceVideoOutConfigureOutput(display_handle, hz == 120 ? 15 : 1, 0, 0, 0);
    if (rc != 0) {
        // A failed mode switch can partially change the output; restore it.
        int restore = sceVideoOutConfigureOutput(display_handle, previous == 120 ? 15 : 1, 0, 0, 0);
        fprintf(stderr, "[yataidon-ps5] refresh=%d failed=%08x restore=%08x\n", hz, (unsigned)rc, (unsigned)restore);
        return restore == 0 ? (int)previous : -1;
    }
    changed_live = 1;
    ps5_scanout_fps = ps5_scanout_active_fps = (unsigned)hz;
    sceVideoOutWaitVblank(display_handle);
    sceVideoOutWaitVblank(display_handle);
    fprintf(stderr, "[yataidon-ps5] live output refresh=%d Hz\n", hz);
    return hz;
}

int __wrap_sceVideoOutClose(int32_t handle) {
    if (handle == display_handle && changed_live) {
        // Also covers a title that started at 60 Hz, whose SDK restore guard
        // was never armed by its initial presenter setup.
        int rc = sceVideoOutConfigureOutput(handle, 1, 0, 0, 0);
        if (rc != 0) return rc;
        sceVideoOutWaitVblank(handle);
        sceVideoOutWaitVblank(handle);
    }
    int rc = __real_sceVideoOutClose(handle);
    if (rc == 0 && handle == display_handle) {
        display_handle = -1;
        changed_live = 0;
    }
    return rc;
}
