/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ghost_note.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "looper.h"

#define DENSITY_WIN_HALF 8
#define CHANCE(p) ((rand() / (RAND_MAX + 1.0)) < (p))

static float note_density_track_window[4][LOOPER_TOTAL_STEPS];

static bool pending_fill_request = false;

static ghost_parameters_t parameters = {
    .ghost_intensity = 0.843,
    .swing_ratio = 0.5,
    .boundary = {.before_probability = 0.10, .after_probability = 0.50},
    .euclidean = {.k_max = 16, .k_sufficient = 6, .k_intensity = 0.90, .probability = 0.80},
    .fill = {.interval_bar = 4, .start_mean = 15.0, .start_sd = 5.0, .probability = 0.40},
};

ghost_parameters_t *ghost_note_parameters(void) { return &parameters; }

static uint8_t velocity_table[] = {
    0x20,  // track 1 - Kick
    0x25,  // track 2 - Snare
    0x30,  // track 3 - Hi-hat closed
    0x25   // track 4 - Hi-hat open
};

uint8_t *ghost_note_velocity_table(void) { return velocity_table; }

#define KICK_VEL_BASE 100
#define KICK_VEL_DEPTH 25
#define HH_FREQ_RATIO 2
#define HH_VEL_BASE 107
#define HH_VEL_DEPTH 20

uint8_t ghost_note_modulate_base_velocity(uint8_t track_num, uint8_t default_velocity, float lfo) {
    if (track_num == 0) {                                    // Kick
        float phase = (lfo * 1.25 / 65536.0f) * 2.0f * M_PI; /* 0-2π */
        float kick_s = sinf(phase);
        return KICK_VEL_BASE + (int)(kick_s * KICK_VEL_DEPTH);
    } else if (track_num == 2) {                           // Closed Hi-hat
        uint16_t hh_phase = (uint32_t)lfo * HH_FREQ_RATIO; /* wrap */
        float hh_s = sinf((hh_phase / 65536.0f) * 2.0f * M_PI);
        return HH_VEL_BASE + (int)(hh_s * HH_VEL_DEPTH);
    }
    return default_velocity;
}

float ghost_note_modulate_swing_ratio(float lfo) {
    float gi = parameters.ghost_intensity;
    float swing;
    if (gi < 0.5f) {
        swing = 0.5f;
    } else {
        float t = (gi - 0.5f) * 2.0f;
        float base = 0.5f + powf(t, 7.0f) * 0.15f;

        float phase = ((uint32_t)lfo / 65536.0f) * 2.0f * M_PI;
        float lfo_amt = sinf(phase + M_PI_2) * 0.01f;
        swing = base + lfo_amt;

        if (swing > 0.65f)
            swing = 0.65f;
        if (swing < 0.5f)
            swing = 0.5f;
    }
    return swing;
}

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

static double rand_normal(double mu, double sigma2) {
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

// Count existing user notes
static uint8_t count_user_notes(const bool note_pattern[], size_t len) {
    uint8_t n = 0;
    for (size_t i = 0; i < len; i++) {
        if (note_pattern[i])
            n++;
    }
    return n;
}

// Determine how many extra notes to add
static uint8_t calculate_extra_note_count(uint8_t current) {
    euclidean_parameters_t *euclid = &parameters.euclidean;

    if (current >= euclid->k_sufficient)
        return 0;

    float ratio = (euclid->k_sufficient - current) / (float)euclid->k_sufficient;
    return ceilf(ratio * euclid->k_intensity * (euclid->k_max - current));
}

// Apply the ghost notes
static void apply_euclidean_ghost_notes(track_t *track, uint8_t total_notes, uint8_t offset) {
    euclidean_parameters_t *euclid = &parameters.euclidean;
    float density = total_notes / (float)LOOPER_TOTAL_STEPS;
    uint32_t euclid_accumulator = 0;

    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        euclid_accumulator += total_notes;
        if (euclid_accumulator >= LOOPER_TOTAL_STEPS) {
            euclid_accumulator -= LOOPER_TOTAL_STEPS;
            size_t pos = (i + offset) % LOOPER_TOTAL_STEPS;

            if (!track->pattern[pos] && track->ghost_notes[pos].rand_sample == 0) {
                float probability = euclid->probability * (1.0f - density);
                uint8_t prob = (uint8_t)roundf(clamp_int(probability * 100.0f, 0, 100));
                track->ghost_notes[pos].probability = prob;
                track->ghost_notes[pos].rand_sample = rand() % 100;
            }
        }
    }
}

// Add Euclidean ghost notes to the track
static void add_euclidean_ghost_notes(track_t *track) {
    euclidean_parameters_t *euclid = &parameters.euclidean;

    uint8_t n = count_user_notes(track->pattern, LOOPER_TOTAL_STEPS);
    if (n == 0 || n >= LOOPER_TOTAL_STEPS)
        return;

    uint8_t extra_note_count = calculate_extra_note_count(n);
    uint8_t target_note_count = clamp_int(n + extra_note_count, 1, euclid->k_max);

    uint8_t phase_step_count = LOOPER_TOTAL_STEPS / target_note_count;
    uint8_t phase_offset = rand() % phase_step_count;

    apply_euclidean_ghost_notes(track, target_note_count, phase_offset);
}

// 1/16th positions around the user input
static void add_boundary_notes(track_t *track) {
    boundary_parameters_t *boundary = &parameters.boundary;

    for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) {
        if (track->pattern[i] &&
            !track->pattern[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_notes[i].rand_sample) {
            track->ghost_notes[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS].probability =
                (uint8_t)(boundary->before_probability * 100);
            track->ghost_notes[(LOOPER_TOTAL_STEPS + i - 1) % LOOPER_TOTAL_STEPS].rand_sample =
                rand() % 100;
        }
        if (track->pattern[i] && !track->pattern[(i + 1) % LOOPER_TOTAL_STEPS] &&
            !track->ghost_notes[i].rand_sample) {
            track->ghost_notes[(i + 1) % LOOPER_TOTAL_STEPS].probability =
                (uint8_t)(boundary->after_probability * 100);
            track->ghost_notes[(i + 1) % LOOPER_TOTAL_STEPS].rand_sample = rand() % 100;
        }
    }
}

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
            note_density_track_window[t][i] = track_window_density(&tracks[t], i, DENSITY_WIN_HALF);
        }
    }
}

// Add ghost fill-in notes based on track density and randomized start
static void add_fillin_notes(void) {
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    fill_parameters_t *fill = &parameters.fill;

    update_density_track_window();

    uint16_t fill_start =
        LOOPER_TOTAL_STEPS - abs((int8_t)rand_normal(fill->start_mean, fill->start_sd));
    for (size_t t = 0; t < num_tracks; t++) {
        if (t != 0 && t != 1)
            continue;

        for (size_t i = fill_start; i < LOOPER_TOTAL_STEPS; i++) {
            bool ghost_on = (float)(tracks[t].ghost_notes[i].probability / 100.0f) *
                                parameters.ghost_intensity >
                            (float)tracks[t].ghost_notes[i].rand_sample / 100.0f;
            if (!ghost_on) {
                tracks[t].ghost_notes[i].probability =
                    (uint8_t)((1.0 - note_density_track_window[t][i]) * 0.25 * 100.0f);
                tracks[t].ghost_notes[i].rand_sample = rand() % 100;
            }
            if (((float)tracks[t].ghost_notes[i].probability / 100.0f) *
                    parameters.ghost_intensity >
                (float)(tracks[t].ghost_notes[i].rand_sample / 100))
                tracks[t].fill_pattern[i] = CHANCE(fill->probability * parameters.ghost_intensity);
        }
    }
}

static void add_fillin_notes_now(void) {
    looper_status_t *looper_status = looper_status_get();
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    fill_parameters_t *fill = &parameters.fill;

    update_density_track_window();

    uint16_t fill_start = looper_status->current_step;
    for (size_t t = 0; t < num_tracks; t++) {
        if (t != 0 && t != 1)
            continue;

        for (size_t i = fill_start; i < LOOPER_TOTAL_STEPS; i++) {
            bool ghost_on = (float)(tracks[t].ghost_notes[i].probability / 100.0f) *
                                parameters.ghost_intensity >
                            (float)tracks[t].ghost_notes[i].rand_sample / 100.0f;
            if (!ghost_on) {
                tracks[t].ghost_notes[i].probability =
                    (uint8_t)((1.0 - note_density_track_window[t][i]) * 0.25 * 100.0f);
                tracks[t].ghost_notes[i].rand_sample = rand() % 100;
            }
            if (((float)tracks[t].ghost_notes[i].probability / 100.0f) *
                    parameters.ghost_intensity >
                (float)(tracks[t].ghost_notes[i].rand_sample / 100))
                tracks[t].fill_pattern[i] = CHANCE(fill->probability * parameters.ghost_intensity);
        }
    }
}

static float pattern_density(void) {
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    uint8_t n = 0;

    for (size_t t = 0; t < num_tracks; t++) {
        for (size_t i = 0; i < LOOPER_TOTAL_STEPS; i++) n += (uint8_t)tracks[t].pattern[i];
    }
    return n / (float)(num_tracks * LOOPER_TOTAL_STEPS);
}

void ghost_note_create(track_t *track) {
    memset(track->ghost_notes, 0, sizeof(track->ghost_notes));

    add_euclidean_ghost_notes(track);
    add_boundary_notes(track);
}

static inline bool is_first_step(looper_status_t *s) { return s->current_step == 0; }

static inline bool is_bar_start(looper_status_t *s) {
    return s->current_step % (LOOPER_BEATS_PER_BAR * LOOPER_STEPS_PER_BEAT) == 0;
}

static inline bool is_creation_bar(looper_status_t *s) { return s->ghost_bar_counter == 0; }

static inline bool is_fillin_bar(looper_status_t *s) {
    fill_parameters_t *fill = &parameters.fill;
    return s->ghost_bar_counter == (fill->interval_bar - 2);
}

void ghost_note_set_pending_fill_request(void) { pending_fill_request = true; }

void ghost_note_maintenance_step(void) {
    looper_status_t *looper_status = looper_status_get();
    size_t num_tracks;
    track_t *tracks = looper_tracks_get(&num_tracks);
    fill_parameters_t *fill = &parameters.fill;

    if (is_bar_start(looper_status))
        looper_status->ghost_bar_counter =
            (looper_status->ghost_bar_counter + 1) % fill->interval_bar;

    if (is_first_step(looper_status)) {
        for (size_t i = 0; i < num_tracks; i++)
            memset(tracks[i].fill_pattern, 0, sizeof(tracks[i].fill_pattern));
    }

    if (is_creation_bar(looper_status) && is_first_step(looper_status)) {
        for (size_t i = 0; i < num_tracks; i++) {
            ghost_note_create(&tracks[i]);
        }
    } else if (is_fillin_bar(looper_status) && is_first_step(looper_status) &&
               looper_status->state == LOOPER_STATE_PLAYING) {
        if (pattern_density() > 0)
            add_fillin_notes();
    } else if (pending_fill_request) {
        add_fillin_notes_now();
        pending_fill_request = false;
    }

    parameters.swing_ratio = ghost_note_modulate_swing_ratio(looper_status->lfo_phase);
}
