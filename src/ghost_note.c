#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "looper.h"
#include "ghost_note.h"

#define PROBABILITY(p) ((rand() / (RAND_MAX + 1.0)) < (p))

static ghost_parameters_t parameters = {
    .flams = {
        .post_probability = 0.70,
        .after_probability = 0.30},
    .euclidean = {
        .k_max = 16,
        .k_sufficient = 6,
        .k_intensity = 0.60,
        .probability = 0.70},
    .fill = {
        .start_mean = 15.0,
        .start_sd = 5.0,
        .probability = 0.75},
};

static uint8_t velocity_table[] = {
    0x20,  // track 1 - Kick
    0x25,  // track 2 - Snare
    0x30,  // track 3 - Hi-hat closed
    0x25   // track 4 - Hi-hat open
};

uint8_t *ghost_note_velocity_table(void) { return velocity_table; }

static double rand_standard_normal(void) {
    static int has_spare = 0;
    static double spare;
    if (has_spare) {
        has_spare = 0;
        return spare;
    }
    has_spare = 1;

    double u, v, s;
    do {
        u = rand() / (double)RAND_MAX * 2.0 - 1.0;
        v = rand() / (double)RAND_MAX * 2.0 - 1.0;
        s = u * u + v * v;
    } while (s >= 1.0 || s == 0.0);

    s = sqrt(-2.0 * log(s) / s);
    spare = v * s;
    return u * s;
}

double rand_normal(double mu, double sigma2) {
    double sigma = sqrt(sigma2);
    return mu + sigma * rand_standard_normal();
}

static inline int clamp_int(int x, int lo, int hi) {
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

// euclidean rhythm algorithm
static void add_ghost_euclidean(track_t *track) {
    euclidean_parameters_t *euclidean = &parameters.euclidean;

    uint8_t k = 0;
    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i])
            k++;
    }
    if (k == 0 || k >= LOOPER_TOTAL_STEPS)
        return;

    uint8_t k_base = k;
    uint8_t k_auto = 0;
    if (k < euclidean->k_sufficient) {
        float ratio = (euclidean->k_sufficient - k) / (float)euclidean->k_sufficient;
        k_auto = ceilf(ratio * euclidean->k_intensity * (euclidean->k_max - k));
    }
    k = clamp_int(k_base + k_auto, 1, euclidean->k_max);

    uint8_t max_phase = LOOPER_TOTAL_STEPS / k;
    uint8_t phase = rand() % max_phase;
    float density = (float)k / (float)LOOPER_TOTAL_STEPS;
    uint8_t bucket = 0;
    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        bucket += k;
        if (bucket >= LOOPER_TOTAL_STEPS) {
            bucket -= LOOPER_TOTAL_STEPS;
            size_t pos = (i + phase) % LOOPER_TOTAL_STEPS;

            if (!track->pattern[pos] && !track->ghost_pattern[pos])
                track->ghost_pattern[pos] = PROBABILITY(euclidean->probability * (1.0f - density));
        }
    }
}

// 1/16th positions around the user input
static void add_ghost_flams(track_t *track) {
    flams_parameters_t *flams = &parameters.flams;

    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i] && !track->pattern[(i + 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i])
            track->ghost_pattern[(i + 1) % LOOPER_TOTAL_STEPS] =
                PROBABILITY(flams->after_probability);
        if (track->pattern[i] &&
            !track->pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i])
            track->ghost_pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] =
                PROBABILITY(flams->post_probability);
    }
}

void ghost_note_create(track_t *track) {
    memset(track->ghost_pattern, 0, LOOPER_TOTAL_STEPS);

    add_ghost_euclidean(track);
    add_ghost_flams(track);
}

void ghost_note_maintenance_step(void) {
    fill_parameters_t *fill = &parameters.fill;
    looper_status_t *looper_status = looper_status_get();
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);

    if (looper_status->current_step % (LOOPER_TOTAL_STEPS / 2) == 0)
        looper_status->ghost_bar_counter = (looper_status->ghost_bar_counter + 1) % 4;

    if (looper_status->ghost_bar_counter == 0 && looper_status->current_step == 0) {
        for (size_t i = 0; i < num_tracks; i++) {
            ghost_note_create(&tracks[i]);
            memset(tracks[i].fill_pattern, 0, sizeof(tracks[i].fill_pattern));
        }
    } else if (looper_status->ghost_bar_counter == 2 && looper_status->current_step == 0) {
        uint16_t fill_start = LOOPER_TOTAL_STEPS - abs(rand_normal(fill->start_mean, fill->start_sd));
        for (size_t i = 0; i < num_tracks; i++) {
            for (size_t f = fill_start; f < LOOPER_TOTAL_STEPS; f++) {
                if (tracks[i].ghost_pattern[f])
                    tracks[i].fill_pattern[f] = PROBABILITY(fill->probability);
            }
        }
    }
}

ghost_parameters_t *ghost_note_parameters(void) {
    return &parameters;
}
