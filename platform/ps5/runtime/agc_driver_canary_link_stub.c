/*
 * PS5 linked-AGC canary driver profile.
 * Copyright (C) 2026 Mihawk-99
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host-link only: never packaged or executed.
 */

#include <stdint.h>

int32_t sceAgcDriverSubmitDcb(void *description)
{
    (void)description;
    return -1;
}

int32_t sceAgcDriverSetTFRing(uintptr_t address, uint32_t size)
{
    (void)address;
    (void)size;
    return -1;
}

int32_t sceAgcDriverGetTFRing(uintptr_t *address, uint32_t *size)
{
    (void)address;
    (void)size;
    return -1;
}

int32_t sceAgcDriverSetHsOffchipParam(uint32_t granularity, uint32_t buffering)
{
    (void)granularity;
    (void)buffering;
    return -1;
}

int32_t sceAgcDriverGetHsOffchipParam(uint16_t *granularity, uint16_t *buffering)
{
    (void)granularity;
    (void)buffering;
    return -1;
}

uint32_t sceAgcDriverGetWaitRenderingPacketSizeInDwords(void)
{
    return 0;
}

uint32_t sceAgcDriverWaitUntilSafeForRendering(uint32_t **command, uint32_t packet_size,
                                               uint32_t reserved, uint32_t handle,
                                               int buffer_index)
{
    (void)command;
    (void)packet_size;
    (void)reserved;
    (void)handle;
    (void)buffer_index;
    return 0;
}
