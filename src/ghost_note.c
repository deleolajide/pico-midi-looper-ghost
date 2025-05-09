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
