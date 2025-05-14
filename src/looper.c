/*
 * looper.c
 *
 * Core looper module: Implements a 2-bar step sequencer driven by timer ticks
 * and button input. Exposes functions for processing sequencer steps,
 * handling timer ticks, and handling user input events.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>
#include <string.h>

#include "drivers/async_timer.h"
#include "drivers/ble_midi.h"
#include "drivers/button.h"
#include "drivers/display.h"
#include "drivers/led.h"
#include "drivers/usb_midi.h"
#include "looper.h"
#include "ghost_note.h"
#include "tap_tempo.h"

enum {
    MIDI_CHANNEL1 = 0,
    MIDI_CHANNEL10 = 9,
};

enum {
    BASS_DRUM = 36,
    RIM_SHOT = 37,
    SNARE_DRUM = 38,
    HAND_CLAP = 39,
    CLOSED_HIHAT = 42,
    OPEN_HIHAT = 46,
    CYMBAL = 49,
};

static looper_status_t looper_status = {.bpm = LOOPER_DEFAULT_BPM, .state = LOOPER_STATE_WAITING};

static track_t tracks[] = {
    {"Bass", BASS_DRUM, MIDI_CHANNEL10, {0}, {0}},
    {"Snare", SNARE_DRUM, MIDI_CHANNEL10, {0}, {0}},
    {"Hi-hat", CLOSED_HIHAT, MIDI_CHANNEL10, {0}, {0}},
    {"Open Hi-hat", OPEN_HIHAT, MIDI_CHANNEL10, {0}, {0}},
};
static const size_t NUM_TRACKS = sizeof(tracks) / sizeof(track_t);

// Check if the note output destination is ready.
static bool looper_perform_ready(void) {
    return usb_midi_is_connected() || ble_midi_is_connected();
}

// Send a note event to the output destination.
static void looper_perform_note(uint8_t channel, uint8_t note, uint8_t velocity) {
    usb_midi_send_note(channel, note, velocity);
    ble_midi_send_note(channel, note, velocity);
}

// Sends a MIDI click at specific steps to indicate rhythm.
static void send_click_if_needed(void) {
    if ((looper_status.current_step % LOOPER_CLICK_DIV) == 0 && looper_status.current_step == 0)
        looper_perform_note(MIDI_CHANNEL1, RIM_SHOT, 0x30);
    else if ((looper_status.current_step % LOOPER_CLICK_DIV) == 0)
        looper_perform_note(MIDI_CHANNEL1, RIM_SHOT, 0x10);
}

// Perform all note events for the current step across all tracks.
// If the current track is active, also update the status LED.
static void looper_perform_step(void) {
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].pattern[looper_status.current_step];
        if (note_on) {
            uint8_t velocity = ghost_note_modulate_base_velocity(i, 0x7f, looper_status.lfo_phase);
            looper_perform_note(tracks[i].channel, tracks[i].note, velocity);
            if (i == looper_status.current_track)
                led_set(1);
        } else if (i == looper_status.current_track) {
            led_set(0);
        }

        uint8_t *ghost_note_velocity = ghost_note_velocity_table();
        if (tracks[i].ghost_pattern[looper_status.current_step] &&
            !tracks[i].fill_pattern[looper_status.current_step])
            looper_perform_note(tracks[i].channel, tracks[i].note, ghost_note_velocity[i]);
        if (tracks[i].fill_pattern[looper_status.current_step] && !note_on)
            looper_perform_note(tracks[i].channel, tracks[i].note, 0x7f);
    }
}

// Perform note events for the current step while recording.
// In recording mode, the status LED is always turned on.
static void looper_perform_step_recording(void) {
    led_set(1);
    for (uint8_t i = 0; i < NUM_TRACKS; i++) {
        bool note_on = tracks[i].pattern[looper_status.current_step];
        if (note_on)
            looper_perform_note(tracks[i].channel, tracks[i].note, 0x7f);
    }
}

// Updates the current step index and timestamp based on current loop progress.
static void looper_next_step(uint64_t now_us) {
    looper_status.timing.last_step_time_us = now_us;
    looper_status.current_step = (looper_status.current_step + 1) % LOOPER_TOTAL_STEPS;
}

/*
 * Returns the step index nearest to the stored `button_press_start_us` timestamp.
 * The result is quantized to the nearest step relative to the last tick.
 */
static uint8_t looper_quantize_step() {
    int64_t delta_us =
        looper_status.timing.button_press_start_us - looper_status.timing.last_step_time_us;
    // Convert to step offset using rounding (nearest step)
    int32_t relative_steps =
        (int32_t)round((double)delta_us / 1000.0 / looper_status.step_duration_ms);
    uint8_t previous_step =
        (looper_status.current_step + LOOPER_TOTAL_STEPS - 1) % LOOPER_TOTAL_STEPS;
    uint8_t estimated_step =
        (previous_step + relative_steps + LOOPER_TOTAL_STEPS) % LOOPER_TOTAL_STEPS;
    return estimated_step;
}

// Clear all patterns in every track
static void looper_clear_all_tracks() {
    for (size_t i = 0; i < NUM_TRACKS; i++) {
        memset(tracks[i].pattern, 0, sizeof(tracks[i].pattern));
        memset(tracks[i].ghost_pattern, 0, sizeof(tracks[i].ghost_pattern));
        memset(tracks[i].fill_pattern, 0, sizeof(tracks[i].fill_pattern));
    }
}

// Routes button events related to tap-tempo mode.
static tap_result_t taptempo_handle_button_event(button_event_t event) {
    tap_result_t result = taptempo_handle_event(event);
    switch (result) {
        case TAP_PRELIM:
        case TAP_FINAL:
            looper_update_bpm(taptempo_get_bpm());
            break;
        case TAP_EXIT: /* leave mode */
            break;
        default:
            break;
    }
    return result;
}

// Return a pointer to the current looper status.
looper_status_t *looper_status_get(void) { return &looper_status; }

track_t *looper_tracks_get(size_t *num) {
    *num = NUM_TRACKS;
    return tracks;
}

// Retrieve the current step interval in milliseconds.
uint32_t looper_get_step_interval_ms(void) { return looper_status.step_duration_ms; }

// Update the looper BPM and recalculate the step duration.
void looper_update_bpm(uint32_t bpm) {
    looper_status.bpm = bpm;
    looper_status.step_duration_ms = 60000 / (bpm * LOOPER_STEPS_PER_BEAT);
}

// Processes the looper's main state machine, called by the step timer.
void looper_process_state(uint64_t start_us) {
    bool ready = looper_perform_ready();
    display_update_looper_status(ready, &looper_status, tracks, NUM_TRACKS);
    if (!ready)
        looper_status.state = LOOPER_STATE_WAITING;
    switch (looper_status.state) {
        case LOOPER_STATE_WAITING:
            if (ready) {
                looper_status.state = LOOPER_STATE_PLAYING;
                looper_status.current_step = 0;
            }
            led_set((looper_status.current_step % (LOOPER_CLICK_DIV * 4)) == 0);
            looper_next_step(start_us);
            break;
        case LOOPER_STATE_PLAYING:
            send_click_if_needed();
            looper_perform_step();
            looper_next_step(start_us);
            break;
        case LOOPER_STATE_RECORDING:
            send_click_if_needed();
            looper_perform_step_recording();
            if (looper_status.recording_step_count >= LOOPER_TOTAL_STEPS) {
                led_set(0);
                looper_status.state = LOOPER_STATE_PLAYING;
            }
            looper_next_step(start_us);
            looper_status.recording_step_count++;
            break;
        case LOOPER_STATE_TRACK_SWITCH:
            looper_status.current_track = (looper_status.current_track + 1) % NUM_TRACKS;
            looper_perform_note(MIDI_CHANNEL10, HAND_CLAP, 0x7f);
            looper_next_step(start_us);
            looper_status.state = LOOPER_STATE_PLAYING;
            break;
        case LOOPER_STATE_TAP_TEMPO:
            send_click_if_needed();
            led_set((looper_status.current_step % LOOPER_CLICK_DIV) == 0);
            looper_next_step(start_us);
            break;
        case LOOPER_STATE_CLEAR_TRACKS:
            looper_clear_all_tracks();
            looper_status.current_track = 0;
            looper_update_bpm(LOOPER_DEFAULT_BPM);
            looper_next_step(start_us);
            looper_status.state = LOOPER_STATE_PLAYING;
        default:
            break;
    }

    looper_status.lfo_phase += LFO_RATE;
    ghost_note_maintenance_step();
}

// Handles button events and updates the looper state accordingly.
void looper_handle_button_event(button_event_t event) {
    track_t *track = &tracks[looper_status.current_track];

    switch (event) {
        case BUTTON_EVENT_DOWN:
            // Button pressed: start timing and preview sound
            looper_status.timing.button_press_start_us = time_us_64();
            looper_perform_note(track->channel, track->note, 0x7f);
            // Backup track pattern in case this press becomes a long-press (undo)
            memcpy(track->hold_pattern, track->pattern, LOOPER_TOTAL_STEPS);
            break;
        case BUTTON_EVENT_CLICK_RELEASE:
            // Short press release: quantize and record step
            if (looper_status.state != LOOPER_STATE_RECORDING) {
                looper_status.recording_step_count = 0;
                looper_status.state = LOOPER_STATE_RECORDING;
                looper_clear_all_tracks();
            }
            uint8_t quantized_step = looper_quantize_step();
            track->pattern[quantized_step] = true;
            break;
        case BUTTON_EVENT_HOLD_RELEASE:
            // Long press release: revert track and switch
            memcpy(track->pattern, track->hold_pattern, LOOPER_TOTAL_STEPS);
            looper_status.state = LOOPER_STATE_TRACK_SWITCH;
            break;
        case BUTTON_EVENT_LONG_HOLD_RELEASE:
            // ≥2 s hold: enter Tap-tempo (no track switch)
            looper_status.state = LOOPER_STATE_TAP_TEMPO;
            looper_perform_note(MIDI_CHANNEL10, HAND_CLAP, 0x7f);
            break;
        case BUTTON_EVENT_VERY_LONG_HOLD_RELEASE:
            // ≥5 s hold: clear track data
            looper_status.state = LOOPER_STATE_CLEAR_TRACKS;
            looper_perform_note(MIDI_CHANNEL10, HAND_CLAP, 0x7f);
            break;
        default:
            break;
    }
}

// Runs `looper_process_state()` and reschedules tick timer.
void looper_handle_tick(async_context_t *ctx, async_at_time_worker_t *worker) {
    uint64_t start_us = time_us_64();

    looper_process_state(start_us);

    // Re-arms the timer to fire again after `step_duration_ms`, adjusting for processing time.
    uint64_t handler_delay_ms = (time_us_64() - start_us) / 1000;
    uint32_t delay = (handler_delay_ms >= looper_status.step_duration_ms)
                         ? 1
                         : looper_status.step_duration_ms - handler_delay_ms;
    async_context_add_at_time_worker_in_ms(ctx, worker, delay);
}

// Poll button events, process them, and update the status LED.
void looper_handle_input(void) {
    button_event_t event = button_poll_event();
    if (looper_status.state == LOOPER_STATE_TAP_TEMPO) {
        if (taptempo_handle_button_event(event) == TAP_EXIT)
            looper_status.state = LOOPER_STATE_PLAYING;
    } else {
        looper_handle_button_event(event);
    }
    led_update();
}

void looper_schedule_step_timer(void) {
    looper_update_bpm(LOOPER_DEFAULT_BPM);

    static async_at_time_worker_t worker;
    worker.do_work = looper_handle_tick;
    async_context_t *ctx = async_timer_async_context();
    async_context_add_at_time_worker_in_ms(ctx, &worker, looper_get_step_interval_ms());
}
