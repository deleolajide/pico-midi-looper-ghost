#pragma once

#include <stdbool.h>
#include <stdint.h>

void note_scheduler_init(void);
bool note_scheduler_schedule_note(uint64_t time_us, uint8_t channel, uint8_t note, uint8_t velocity);
void note_scheduler_dispatch_pending(void);
