#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "looper.h"

#define PROBABILITY(p) ((rand() / (RAND_MAX + 1.0)) < (p))

static uint8_t velocity_table[] = {
    0x20,  // track 1 - Kick
    0x25,  // track 2 - Snare
    0x30,  // track 3 - Hi-hat closed
    0x25   // track 4 - Hi-hat open
};

uint8_t *ghost_note_velocity_table(void) { return velocity_table; }

// euclidean rhythm algorithm
static void add_ghost_euclidean(track_t *track) {
    uint8_t k = 0;
    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i])
            k++;
    }
    float density = (float)k / (float)LOOPER_TOTAL_STEPS;

    if (k == 0 || k >= LOOPER_TOTAL_STEPS)
        return;

    uint8_t max_phase = LOOPER_TOTAL_STEPS / k;
    uint8_t phase = rand() % max_phase;

    uint8_t bucket = 0;
    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        bucket += k;
        if (bucket >= LOOPER_TOTAL_STEPS) {
            bucket -= LOOPER_TOTAL_STEPS;
            size_t pos = (i + phase) % LOOPER_TOTAL_STEPS;

            if (!track->pattern[pos] && !track->ghost_pattern[pos])
                track->ghost_pattern[pos] = PROBABILITY(0.80 * (1.0f - density));
        }
    }
}

// 1/16th positions around the user input
static void add_ghost_flams(track_t *track) {
    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i] && !track->pattern[(i + 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i])
            track->ghost_pattern[(i + 1) % LOOPER_TOTAL_STEPS] = PROBABILITY(0.30);
        if (track->pattern[i] &&
            !track->pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i])
            track->ghost_pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] =
                PROBABILITY(0.30);
    }
}

void ghost_note_create(track_t *track) {
    memset(track->ghost_pattern, 0, LOOPER_TOTAL_STEPS);

    add_ghost_euclidean(track);
    add_ghost_flams(track);
}

void ghost_note_maintenance_step(void) {
    looper_status_t *looper_status = looper_status_get();
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);

    if (looper_status->current_step % (LOOPER_TOTAL_STEPS / 2) == 0)
        looper_status->ghost_bar_counter = (looper_status->ghost_bar_counter + 1) % 4;

    const uint16_t fill_start = (float)LOOPER_TOTAL_STEPS * (3.0 / 4);

    if (looper_status->ghost_bar_counter == 0 && looper_status->current_step == 0) {
        for (size_t i = 0; i < num_tracks; i++) {
            ghost_note_create(&tracks[i]);
            memset(tracks[i].fill_pattern, 0, sizeof(tracks[i].fill_pattern));
        }
    } else if (looper_status->ghost_bar_counter == 2 && looper_status->current_step == 0) {
        for (size_t i = 0; i < num_tracks; i++) {
            for (size_t f = fill_start; f < LOOPER_TOTAL_STEPS; f++) {
                if (tracks[i].ghost_pattern[f]) {
                    tracks[i].fill_pattern[f] = PROBABILITY(0.90);
                }
            }
        }
    }
}
