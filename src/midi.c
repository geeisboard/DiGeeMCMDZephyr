#include "midi.h"
#include "syscalls/uart.h"
#include "zephyr/device.h"

int midi_init(midi_dev_t *dev, const struct device *uart)
{
    if (!device_is_ready(uart)) return -ENODEV;
    dev->uart = uart;
    return 0;
}

static void send_byte(midi_dev_t *dev, uint8_t byte)
{
    uart_poll_out(dev->uart, byte);
}

void midi_clock(midi_dev_t *dev) {send_byte(dev, MIDI_CLOCK); }
void midi_start(midi_dev_t *dev) {send_byte(dev, MIDI_START); }
void midi_stop(midi_dev_t *dev)  {send_byte(dev, MIDI_STOP);  }