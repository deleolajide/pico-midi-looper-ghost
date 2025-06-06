/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "looper.h"

typedef struct {
    uint8_t k_max;
    uint8_t k_sufficient;
    float k_intensity;
    float probability;
} euclidean_parameters_t;

typedef struct {
    float before_probability;
    float after_probability;
} boundary_parameters_t;

typedef struct {
    uint8_t interval_bar;
    float start_mean;
    float start_sd;
    float probability;
} fill_parameters_t;

typedef struct {
    float ghost_intensity;
    float swing_ratio;
    float swing_ratio_base;
    boundary_parameters_t boundary;
    euclidean_parameters_t euclidean;
    fill_parameters_t fill;
} ghost_parameters_t;

uint8_t *ghost_note_velocity_table(void);

uint8_t ghost_note_modulate_base_velocity(uint8_t track_num, uint8_t default_velocity, float lfo);

float ghost_note_modulate_swing_ratio(float lfo);

void ghost_note_create(track_t *track);

void ghost_note_maintenance_step(void);

ghost_parameters_t *ghost_note_parameters(void);

void ghost_note_set_pending_fill_request(void);
