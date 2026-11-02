/*
 * PS5 linked-AGC canary import stub.
 * Copyright (C) 2026 Mihawk-99
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Keep this file separate from agc_link_stub.c: experimental imports must not
 * change the stable probe host's native import surface.
 */

#include <stdint.h>

int32_t sceAgcInit(void *state, uint32_t size) { (void)state; (void)size; return -1; }
void *sceAgcGetRegisterDefaults(void) { return 0; }
int32_t sceAgcCreateShader(void **shader, void *header, void *code)
{
    (void)shader;
    (void)header;
    (void)code;
    return -1;
}
int32_t sceAgcLinkShaders(void *cx, void *uc, void *reserved, void *vertex_shader,
                          void *pixel_shader, uint32_t primitive_type)
{
    (void)cx;
    (void)uc;
    (void)reserved;
    (void)vertex_shader;
    (void)pixel_shader;
    (void)primitive_type;
    return -1;
}
int32_t sceAgcSuspendPoint(void)
{
    return -1;
}
uint32_t *sceAgcDcbSetCxRegistersIndirect(void *command, const void *registers, uint32_t count)
{
    (void)command;
    (void)registers;
    (void)count;
    return 0;
}
uint32_t *sceAgcDcbSetUcRegistersIndirect(void *command, const void *registers, uint32_t count)
{
    (void)command;
    (void)registers;
    (void)count;
    return 0;
}
uint32_t *sceAgcDcbSetShRegistersIndirect(void *command, const void *registers, uint32_t count)
{
    (void)command;
    (void)registers;
    (void)count;
    return 0;
}
uint32_t *sceAgcDcbDrawIndexAuto(void *command, uint32_t count, uint64_t modifier)
{
    (void)command;
    (void)count;
    (void)modifier;
    return 0;
}

/* Phase C2: the instance count a draw runs, which libSceAgc provides; the
 * console's instancing probe measures the packet it writes. */
uint32_t *sceAgcDcbSetNumInstances(void *command, uint32_t count)
{
    (void)command;
    (void)count;
    return 0;
}
uint32_t *sceAgcDcbSetIndexSize(void *command, uint8_t index_size, uint8_t reserved)
{
    (void)command;
    (void)index_size;
    (void)reserved;
    return 0;
}
uint32_t *sceAgcDcbSetIndexBuffer(void *command, void *indices)
{
    (void)command;
    (void)indices;
    return 0;
}
uint32_t *sceAgcDcbSetIndexCount(void *command, uint32_t count)
{
    (void)command;
    (void)count;
    return 0;
}
uint32_t *sceAgcDcbDrawIndex(void *command, uint32_t count, void *indices, uint64_t modifier)
{
    (void)command;
    (void)count;
    (void)indices;
    (void)modifier;
    return 0;
}
uint32_t *sceAgcCbReleaseMem(void *command, uint8_t event, int16_t control, uint64_t arg3,
                             int8_t arg4, void *arg5, uint32_t arg6, uint64_t arg7,
                             uint16_t arg8, uint16_t arg9, int8_t arg10, int32_t arg11)
{
    (void)command;
    (void)event;
    (void)control;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    (void)arg8;
    (void)arg9;
    (void)arg10;
    (void)arg11;
    return 0;
}
uint32_t *sceAgcCbSetShRegisterRangeDirect(void *command, uint32_t offset, const uint32_t *values,
                                           uint32_t count)
{
    (void)command;
    (void)offset;
    (void)values;
    (void)count;
    return 0;
}
uint32_t *sceAgcDcbSetFlip(void *command, uint32_t handle, int buffer_index, uint32_t flip_mode,
                           int64_t flip_argument)
{
    (void)command;
    (void)handle;
    (void)buffer_index;
    (void)flip_mode;
    (void)flip_argument;
    return 0;
}
