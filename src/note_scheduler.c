#include <stdint.h>
#include "pico/multicore.h"
#include "pico/time.h"
#include "note_scheduler.h"
#include "looper.h"

#define NOTE_TIMER_INTERVAL_US 1000
#define MAX_SCHEDULED_NOTES 16

typedef struct {
    uint64_t time_us;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
} note_scheduler_note_t;

static repeating_timer_t note_timer;

static note_scheduler_note_t note_queue[MAX_SCHEDULED_NOTES];
static size_t note_queue_len = 0;
static note_scheduler_note_t send_queue[MAX_SCHEDULED_NOTES];
static size_t send_queue_len = 0;
static critical_section_t send_queue_cs;

static bool note_timer_callback(repeating_timer_t *t) {
    (void)t;
    uint64_t now = time_us_64();
    for (size_t i = 0; i < note_queue_len;) {
        if (note_queue[i].time_us <= now) {
            critical_section_enter_blocking(&send_queue_cs);
            if (send_queue_len < MAX_SCHEDULED_NOTES)
                send_queue[send_queue_len++] = note_queue[i];
            critical_section_exit(&send_queue_cs);

            for (size_t j = i; j < note_queue_len - 1; j++) {
                note_queue[j] = note_queue[j + 1];
            }
            if (note_queue_len > 0)
                note_queue_len--;
        } else {
            i++;
        }
    }
    return true;
}

void note_scheduler_start_timer(void) {
    critical_section_init(&send_queue_cs);
    add_repeating_timer_us(-NOTE_TIMER_INTERVAL_US, note_timer_callback, NULL, &note_timer);
}

bool note_scheduler_schedule_note(uint64_t time_us, uint8_t channel, uint8_t note, uint8_t velocity) {
    uint32_t irq_state = save_and_disable_interrupts();

    if (note_queue_len < MAX_SCHEDULED_NOTES) {
        note_queue[note_queue_len++] = (note_scheduler_note_t){
            .time_us = time_us,
            .channel = channel,
            .note = note,
            .velocity = velocity
        };
        restore_interrupts(irq_state);
        return true;
    }
    restore_interrupts(irq_state);
    return false;
}

void note_scheduler_flush_notes(void) {
    critical_section_enter_blocking(&send_queue_cs);
    for (size_t i = 0; i < send_queue_len; i++)
        looper_perform_note(send_queue[i].channel, send_queue[i].note, send_queue[i].velocity);
    send_queue_len = 0;
    critical_section_exit(&send_queue_cs);
}
