/*
 * display.c
 *
 * UART-based text UI for the Pico MIDI Looper.
 * This module is responsible for rendering the USB connection status,
 * current looper state, and per-track step patterns over a serial console.
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdbool.h>
#include <stdio.h>

#include "ghost_note.h"
#include "looper.h"

#define ANSI_BLACK "\x1b[30m"
#define ANSI_BRIGHT_BLACK "\x1b[90m"
#define ANSI_BRIGHT_BLUE "\x1b[94m"
#define ANSI_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_CALM_GREEN "\x1b[38;5;64m"
#define ANSI_SOFT_RED "\x1b[38;5;160m"
#define ANSI_BG_WHITE "\x1b[47m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_RESET "\x1b[0m"

#define ANSI_CLEAR_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME "\x1b[H"
#define ANSI_CLEAR_HOME ANSI_CURSOR_HOME ANSI_CLEAR_SCREEN
#define ANSI_HIDE_CURSOR "\x1b[?25l"
#define ANSI_CURSOR_FMT "\x1b[%u;%uH"
#define ANSI_CLEAR_EOL "\x1b[K"

#define ANSI_ENABLE_ALTSCREEN "\x1b[?1049h"
#define ANSI_DISABLE_ALTSCREEN "\x1b[?1049l"

// Prints a single track row with step highlighting and note indicators.
static void print_track(const track_t *track, uint8_t track_number, bool is_selected) {
    if (is_selected)
        printf("#track %u > %-11s ", track_number + 1, track->name);
    else
        printf("#track %u _ %-11s ", track_number + 1, track->name);

    ghost_parameters_t *params = ghost_note_parameters();
    for (int i = 0; i < LOOPER_TOTAL_STEPS; ++i) {
        bool note_on = track->pattern[i];
        bool ghost_on =
            ((float)track->ghost_notes[i].probability / 100.0f) * params->ghost_intensity >
            (float)track->ghost_notes[i].rand_sample / 100.0f;
        bool fill_on = track->fill_pattern[i];
        if (note_on)
            printf("*");
        else if (fill_on)
            printf("+");
        else if (ghost_on)
            printf(".");
        else
            printf("_");
    }
    printf("\n");
}

static void print_step(uint8_t current_step) {
    printf("#step                  ");
    for (int i = 0; i < LOOPER_TOTAL_STEPS; ++i) {
        if (i == current_step)
            printf("^");
        else
            printf("_");
    }
    printf("\n");
}

// Displays the looper's playback state, connection status, and track patterns.
void display_update_looper_status(bool output_connected, const looper_status_t *looper,
                                  const track_t *tracks, size_t num_tracks) {
    const char *state_label = "WAITING";
    if (output_connected) {
        switch (looper->state) {
            case LOOPER_STATE_PLAYING:
            case LOOPER_STATE_TRACK_SWITCH:
            case LOOPER_STATE_SYNC_PLAYING:
                state_label = "PLAYING";
                break;
            case LOOPER_STATE_RECORDING:
                state_label = "RECORDING";
                break;
            case LOOPER_STATE_TAP_TEMPO:
                state_label = "TAP TEMPO";
                break;
            case LOOPER_STATE_SYNC_MUTE:
                state_label = "MUTE";
                break;
            default:
                break;
        }
    }
    printf("#state %s\n", state_label);

    printf("#bpm %3lu\n", looper->bpm);

    printf("#grid                  1   2   3   4   5   6   7   8\n");

    // Display tracks in order from cymbals to basses, like a typical drum machine.
    for (int8_t i = num_tracks - 1; i >= 0; i--)
        print_track(&tracks[i], i, i == looper->current_track);
    print_step(looper->current_step);
    fflush(stdout);
}
