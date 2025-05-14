#pragma once

#include "looper.h"
#include "ghost_note.h"

typedef struct {
    uint8_t k_max;
    uint8_t k_sufficient;
    float k_intensity;
    float probability;
} euclidean_parameters_t;

typedef struct {
    float before_probability;
    float after_probability;
} flams_parameters_t;

typedef struct {
    uint8_t interval_bar;
    float start_mean;
    float start_sd;
    float probability;
} fill_parameters_t;

typedef struct {
    float ghost_intensity;
    flams_parameters_t flams;
    euclidean_parameters_t euclidean;
    fill_parameters_t fill;
} ghost_parameters_t;

uint8_t *ghost_note_velocity_table(void);

uint8_t ghost_note_modulate_base_velocity(uint8_t track_num, uint8_t default_velocity, float lfo);

void ghost_note_create(track_t *track);

void ghost_note_maintenance_step(void);

ghost_parameters_t *ghost_note_parameters(void);
