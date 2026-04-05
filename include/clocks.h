#ifndef CLOCKS_H
#define CLOCKS_H

#include "midi.h"
#include <stdint.h>
#include <zephyr/kernel.h>
#include <stdbool.h>

#include <zephyr/spinlock.h>

#define CLOCK_TIMER_PRIO  10

#define UCLOCK_INTERNAL 0
#define UCLOCK_EXTERNAL 1

typedef struct {
    midi_dev_t        *midi;
    volatile uint32_t  interval_us;
    volatile uint32_t  tick;
    volatile bool      running;
} midi_clock_dev_t;

#define BPM_TO_US(bpm)         (60000000u / ((bpm) * 24u))
#define POLY_BPM_TO_US_MULT(bpm, num)  (((60000000u) * (16)) / ((bpm) * 24u * (num)))
#define POLY_BPM_TO_US_DIV(bpm, num)   (((60000000u) * (num)) / ((bpm) * 24u * (16)))

int  midi_clock_init(midi_clock_dev_t *dev, midi_dev_t *midi, uint32_t bpm);
void midi_clock_start(midi_clock_dev_t *dev);
void midi_clock_stop(midi_clock_dev_t *dev);
void midi_clock_update(midi_clock_dev_t *dev, uint32_t bpm);
void midi_clock_update_poly(midi_clock_dev_t *dev, uint32_t bpm, bool divide, uint8_t num);
void midi_clock_sync_poly(midi_clock_dev_t *dev, uint32_t bp, bool divide, uint8_t num);

typedef void (*step_cb_t)(uint32_t step);
void midi_clock_set_step_callback(step_cb_t cb);

void midi_clock_set_mode(uint8_t mode);
void midi_clock_clock_me(void);
float midi_clock_get_tempo(void);

void midi_clock_poll(void);

#endif
