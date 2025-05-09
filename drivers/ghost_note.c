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
    int8_t first = -1;

    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i]) {
            if (first == -1)
                first = i;
            k++;
        }
    }

    if (k == 0 || k >= LOOPER_TOTAL_STEPS)
        return;
    for (uint8_t j = 0; j < k; j++) {
        size_t pos = (j * LOOPER_TOTAL_STEPS / k + first) % LOOPER_TOTAL_STEPS;
        if (!track->pattern[pos] && !track->ghost_pattern[pos]) {
            track->ghost_pattern[pos] = PROBABILITY(0.80);
        }
    }
}

// 1/16th positions around the user input
static void add_ghost_flams(track_t *track) {
    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i] && !track->pattern[(i + 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i] && PROBABILITY(0.40))
            track->ghost_pattern[(i + 1) % LOOPER_TOTAL_STEPS] = 1;
        if (track->pattern[i] &&
            !track->pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i] && PROBABILITY(0.25))
            track->ghost_pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] = 1;
    }
}

void ghost_note_create(track_t *track) {
    memset(track->ghost_pattern, 0, LOOPER_TOTAL_STEPS);

    add_ghost_euclidean(track);
    add_ghost_flams(track);
}
