/*
 * ps5-native-app-boilerplate / ProsperoLight - Video decoder link stub.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Link-time declarations for system libSceVideodec2. These bodies are never
 * packaged or executed; the native module writer emits system imports.
 */

#include <stdint.h>

int32_t sceVideodec2QueryDecoderMemoryInfo(const void *config, void *memory)
{
    (void)config;
    (void)memory;
    return -1;
}

int32_t sceVideodec2QueryComputeMemoryInfo(void *memory)
{
    (void)memory;
    return -1;
}

int32_t sceVideodec2AllocateComputeQueue(const void *config, const void *memory, void **queue)
{
    (void)config;
    (void)memory;
    (void)queue;
    return -1;
}

int32_t sceVideodec2ReleaseComputeQueue(void *queue)
{
    (void)queue;
    return -1;
}

int32_t sceVideodec2CreateDecoder(const void *config, const void *memory, void **decoder)
{
    (void)config;
    (void)memory;
    (void)decoder;
    return -1;
}

int32_t sceVideodec2DeleteDecoder(void *decoder)
{
    (void)decoder;
    return -1;
}

int32_t sceVideodec2MapDirectMemory(void *decoder, const void *memory)
{
    (void)decoder;
    (void)memory;
    return -1;
}

int32_t sceVideodec2Reset(void *decoder)
{
    (void)decoder;
    return -1;
}

int32_t sceVideodec2Decode(void *decoder, void *input, void *frame, void *output)
{
    (void)decoder;
    (void)input;
    (void)frame;
    (void)output;
    return -1;
}

int32_t sceVideodec2Flush(void *decoder, void *frame, void *output)
{
    (void)decoder;
    (void)frame;
    (void)output;
    return -1;
}
