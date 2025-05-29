/*
 * note_scheduler.c
 *
 * This module provides precise scheduling of MIDI notes to be played at
 * specific timestamps. It uses async_context to register time-based callbacks
 * and defers actual note execution to the main loop for safe USB transmission.
 *
 * Note: This separation avoids USB mutex contention and ensures timing consistency
 *       without relying on hardware interrupts.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "note_scheduler.h"

#include "drivers/async_timer.h"
#include "looper.h"
#include "pico/multicore.h"
#include "pico/time.h"

#define MAX_SCHEDULED_NOTES 24

// One-time pending note event to be dispatched from the main loop
typedef struct {
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    bool valid;
} pending_note_t;

// Scheduled note with its async worker and parameters
typedef struct {
    async_at_time_worker_t worker;
    pending_note_t pending;
} scheduled_note_slot_t;

static scheduled_note_slot_t scheduled_slots[MAX_SCHEDULED_NOTES];
static pending_note_t pending_notes[MAX_SCHEDULED_NOTES];
static critical_section_t pending_notes_cs;

// Initialize the note scheduler
void note_scheduler_init(void) { critical_section_init(&pending_notes_cs); }

/*
 * Worker callback invoked by async_context at the scheduled time.
 * Adds the pending note to the pending_notes to be executed from the main loop.
 */
static void note_worker_enqueue_pending(async_context_t *ctx, async_at_time_worker_t *worker) {
    (void)ctx;
    scheduled_note_slot_t *slot = (scheduled_note_slot_t *)worker;

    critical_section_enter_blocking(&pending_notes_cs);
    for (size_t i = 0; i < MAX_SCHEDULED_NOTES; i++) {
        if (!pending_notes[i].valid) {
            pending_notes[i] = (pending_note_t){slot->pending.channel, slot->pending.note,
                                                slot->pending.velocity, true};
            break;
        }
    }

    slot->worker.do_work = NULL;  // mark as unused
    critical_section_exit(&pending_notes_cs);
}

/*
 * Schedule a note to be triggered at a specific absolute time in microseconds.
 * Returns false if the scheduling queue is full.
 */
bool note_scheduler_schedule_note(uint64_t time_us, uint8_t channel, uint8_t note,
                                  uint8_t velocity) {
    absolute_time_t note_at = to_us_since_boot(time_us);

    for (size_t i = 0; i < MAX_SCHEDULED_NOTES; i++) {
        if (scheduled_slots[i].worker.do_work == NULL) {
            scheduled_slots[i] = (scheduled_note_slot_t){
                .pending = {.channel = channel, .note = note, .velocity = velocity},
                .worker = {.do_work = note_worker_enqueue_pending}};
            async_context_add_at_time_worker_at(async_timer_async_context(),
                                                &scheduled_slots[i].worker, note_at);
            return true;
        }
    }
    return false;
}

// Called from the main loop to process all pending scheduled notes.
void note_scheduler_dispatch_pending(void) {
    critical_section_enter_blocking(&pending_notes_cs);
    for (size_t i = 0; i < MAX_SCHEDULED_NOTES; i++) {
        if (pending_notes[i].valid) {
            looper_perform_note(pending_notes[i].channel, pending_notes[i].note,
                                pending_notes[i].velocity);
            pending_notes[i].valid = false;
        }
    }
    critical_section_exit(&pending_notes_cs);
}
