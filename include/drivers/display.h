/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "looper.h"

void display_update_looper_status(bool ble_connected, const looper_status_t *looper,
                                  const track_t *tracks, size_t num_tracks);
