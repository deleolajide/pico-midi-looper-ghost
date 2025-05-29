/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <string.h>

#include "hardware/flash.h"
#include "looper.h"
#include "pico/flash.h"

#ifndef GHOST_FLASH_BANK_STORAGE_OFFSET
#define GHOST_FLASH_BANK_STORAGE_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * 4)
#endif

#define MAGIC_HEADER "GHST"
#define NUM_TRACKS 4

typedef struct {
    uint32_t magic;
    bool pattern[NUM_TRACKS][LOOPER_TOTAL_STEPS];
} storage_pattern_t;

typedef struct {
    bool op_is_erase;
    uintptr_t p0;
    uintptr_t p1;
} mutation_operation_t;

static void flash_bank_perform_operation(void *param) {
    const mutation_operation_t *mop = (const mutation_operation_t *)param;
    if (mop->op_is_erase) {
        flash_range_erase(mop->p0, FLASH_SECTOR_SIZE);
    } else {
        flash_range_program(mop->p0, (const uint8_t *)mop->p1, FLASH_PAGE_SIZE);
    }
}

bool storage_load_tracks(void) {
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);

    const storage_pattern_t *data =
        (const storage_pattern_t *)(XIP_BASE + GHOST_FLASH_BANK_STORAGE_OFFSET);
    if (memcmp(&data->magic, MAGIC_HEADER, sizeof(data->magic)) != 0)
        return false;

    for (size_t t = 0; t < num_tracks; t++) {
        for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
            tracks[t].pattern[i] = data->pattern[t][i];
        }
    }
    return true;
}

bool storage_erase_tracks(void) {
    mutation_operation_t erase = {.op_is_erase = true, .p0 = GHOST_FLASH_BANK_STORAGE_OFFSET};
    flash_safe_execute(flash_bank_perform_operation, &erase, UINT32_MAX);
    return true;
}

bool storage_store_tracks(void) {
    uint8_t storage[FLASH_PAGE_SIZE];
    storage_pattern_t *data = (storage_pattern_t *)&storage;
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);

    memcpy(&data->magic, MAGIC_HEADER, sizeof(data->magic));
    for (size_t t = 0; t < NUM_TRACKS; t++) {
        memcpy(data->pattern[t], tracks[t].pattern, sizeof(tracks[t].pattern));
    }
    mutation_operation_t program = {
        .op_is_erase = false, .p0 = GHOST_FLASH_BANK_STORAGE_OFFSET, .p1 = (uintptr_t)storage};
    flash_safe_execute(flash_bank_perform_operation, &program, UINT32_MAX);

    return true;
}
