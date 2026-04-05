#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>
#include <zephyr/drivers/uart.h>

#define MIDI_CLOCK  0xF8
#define MIDI_START  0xFA
#define MIDI_STOP   0xFC

typedef struct {
    const struct device *uart;
} midi_dev_t;

int midi_init(midi_dev_t *dev, const struct device *uart);
void midi_clock(midi_dev_t *dev);
void midi_start(midi_dev_t *dev);
void midi_stop(midi_dev_t *dev);

#endif