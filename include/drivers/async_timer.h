/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "pico/async_context.h"

void async_timer_init(void);

async_context_t *async_timer_async_context(void);
