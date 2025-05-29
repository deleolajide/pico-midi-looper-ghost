/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/async_context.h"

void ble_midi_init() { }

void ble_midi_send_note(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)channel;
    (void)note;
    (void)velocity;
}

bool ble_midi_is_connected(void) { return false; }
