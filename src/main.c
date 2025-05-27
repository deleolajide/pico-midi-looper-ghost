/* main.c
 * Pico MIDI looper for Raspberry Pi Pico.
 * A minimal 2-bars loop recorder using a single button to record and switch tracks.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "pico/stdlib.h"

#include "looper.h"
#include "note_scheduler.h"
#include "drivers/led.h"
#include "drivers/ble_midi.h"
#include "drivers/usb_midi.h"
#include "drivers/async_timer.h"
#include "drivers/storage.h"

/*
 * Entry point for the Pico MIDI Looper application.
 *
 * Looper is driven by two input sources:
 *  - Timer ticks (looper_handle_tick) for sequencer state progression
 *  - Button events (looper_handle_input) for user-driven updates
 */
int main(void) {
    usb_midi_init();
    ble_midi_init();
    stdio_init_all();
    led_init();

    storage_load_tracks();

    // Async timer + sequencer tick setup
    async_timer_init();
    looper_schedule_step_timer();
    note_scheduler_start_timer();

    printf("[MAIN] Pico MIDI Looper start\n");
    while (true) {
        looper_handle_input();
        usb_midi_task();

        note_scheduler_flush_notes();
        tight_loop_contents();
    }
    return 0;
}
