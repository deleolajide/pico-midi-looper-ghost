#include "ghost_note.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "looper.h"

#define PROBABILITY(p) ((rand() / (RAND_MAX + 1.0)) < (p))

static ghost_parameters_t parameters = {
    .ghost_intensity = 1.0,
    .flams = {.before_probability = 0.50, .after_probability = 0.10},
    .euclidean = {.k_max = 16, .k_sufficient = 6, .k_intensity = 0.60, .probability = 0.70},
    .fill = {.start_mean = 15.0, .start_sd = 5.0, .probability = 0.75},
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
static void add_euclidean_notes(track_t *track) {
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
                track->ghost_pattern[pos] = PROBABILITY(euclidean->probability * (1.0f - density) *
                                                        parameters.ghost_intensity);
        }
    }
}

// 1/16th positions around the user input
static void add_flams_notes(track_t *track) {
    flams_parameters_t *flams = &parameters.flams;

    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i] && !track->pattern[(i + 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i])
            track->ghost_pattern[(i + 1) % LOOPER_TOTAL_STEPS] =
                PROBABILITY(flams->before_probability * parameters.ghost_intensity);
        if (track->pattern[i] &&
            !track->pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_pattern[i])
            track->ghost_pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] =
                PROBABILITY(flams->after_probability * parameters.ghost_intensity);
    }
}

void ghost_note_create(track_t *track) {
    memset(track->ghost_pattern, 0, LOOPER_TOTAL_STEPS);

    add_euclidean_notes(track);
    add_flams_notes(track);
}

static float note_density_track_window[4][LOOPER_TOTAL_STEPS];
static float note_density_time_position[LOOPER_TOTAL_STEPS];

static float track_window_density(track_t *track, uint8_t step, uint8_t window) {
    uint8_t n = 0;
    for (int i = -window; i <= window; i++) {
        size_t pos = (LOOPER_TOTAL_STEPS + step + i) % LOOPER_TOTAL_STEPS;
        n += (uint8_t)track->pattern[pos];
    }
    return (float)n / (float)(window * 2 + 1);
}

static void update_density_track_window(void) {
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    for (size_t t = 0; t < num_tracks; t++) {
        for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
            note_density_track_window[t][i] = track_window_density(&tracks[t], i, 6);
        }
    }
}

static void update_density_time_position(void) {
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    for (size_t s = 0; s < LOOPER_TOTAL_STEPS; s++) {
        size_t n = 0;
        for (size_t t = 0; t < num_tracks; t++) {
            n += (uint8_t)tracks[t].pattern[s];
        }
        note_density_time_position[s] = (float)n / (float)num_tracks;
    }
}

static void add_fillin_notes(void) {
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    fill_parameters_t *fill = &parameters.fill;

    update_density_track_window();

    uint16_t fill_start = LOOPER_TOTAL_STEPS - abs(rand_normal(fill->start_mean, fill->start_sd));
    for (size_t t = 0; t < num_tracks; t++) {
        for (size_t i = fill_start; i < LOOPER_TOTAL_STEPS; i++) {
            if ((t == 0 || t == 1) && !tracks[t].ghost_pattern[i]) {
                tracks[t].ghost_pattern[i] = PROBABILITY((1 - note_density_track_window[t][i]) *
                                                         0.25 * parameters.ghost_intensity);
            }
            if (tracks[t].ghost_pattern[i])
                tracks[t].fill_pattern[i] =
                    PROBABILITY(fill->probability * parameters.ghost_intensity);
        }
    }
}

void ghost_note_maintenance_step(void) {
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
        add_fillin_notes();
    }
}

ghost_parameters_t *ghost_note_parameters(void) { return &parameters; }
