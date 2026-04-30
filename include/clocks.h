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

typedef enum {
    MASTER_CLOCK = 0,
    POLY_CLOCK,
} clock_type_t;

typedef struct {
    volatile bool      divide_s;
    volatile bool      paused;
    uint8_t            binary;
    uint8_t            bpm;
} clock_controller_t;

typedef struct {
    midi_dev_t        *midi;
    volatile uint32_t  interval_us;
    volatile uint32_t  next_tick_us;
    volatile uint8_t   tick;
    volatile bool      running;
    clock_type_t       clock_id;  
} midi_clock_dev_t;



#define BPM_TO_US(bpm)         (60000000u / ((bpm) * 24u))
#define POLY_BPM_TO_US_DIV(bpm, num)  (((60000000u) * (16)) / ((bpm) * 24u * (num)))
#define POLY_BPM_TO_US_MULT(bpm, num)   (((60000000u) * (num)) / ((bpm) * 24u * (16)))
uint32_t POLY_BPM_TO_US(clock_controller_t *dev);

int  clock_ctrl_init(clock_controller_t *dev);
void clock_start(clock_controller_t *dev);
void clock_stop(clock_controller_t *dev);
void clock_update(clock_controller_t *dev);

int  midi_clock_init(midi_clock_dev_t *dev, midi_dev_t *midi, clock_type_t type);
void midi_clock_start(midi_clock_dev_t *dev);
void midi_clock_stop(midi_clock_dev_t *dev);
void midi_clock_update(midi_clock_dev_t *dev, clock_controller_t *clock);
void midi_clock_update_poly(midi_clock_dev_t *dev, uint8_t bpm, bool divide, uint8_t binary_prev_num);
void midi_clock_sync_poly(midi_clock_dev_t *dev, uint8_t bpm, bool divide, uint8_t binary_new_num);

typedef void (*step_cb_t)(uint8_t step);
void midi_clock_set_step_callback(step_cb_t cb);

void midi_clock_set_mode(uint8_t mode);
void midi_clock_clock_me(void);
float midi_clock_get_tempo(void);

void midi_clock_poll(void);

#endif
