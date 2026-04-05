#include "clocks.h"
#include "midi.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>
#include <zephyr/spinlock.h>
#include "ssd1306_ctrl.h"

static struct k_spinlock _lock;   // <-- moved here

#define ATOMIC(x) do { \
    k_spinlock_key_t _key = k_spin_lock(&_lock); \
    x; \
    k_spin_unlock(&_lock, _key); \
} while (0)

static midi_clock_dev_t *master_clock_dev = NULL;
static midi_clock_dev_t *poly_clock_dev = NULL;

static step_cb_t step_callback = NULL;

static volatile uint8_t clock_mode = UCLOCK_INTERNAL;
static volatile uint32_t ext_clock_us = 0;
static volatile uint32_t ext_interval = 0;
static volatile float external_tempo = 120.0f;
static uint32_t ext_interval_buffer[64] = {0};
static uint8_t ext_interval_idx = 0;
static uint8_t ext_interval_count = 0;

static uint32_t master_next_tick_us = 0;
static uint32_t poly_next_tick_us = 0;

static inline uint32_t now_us(void)
{
    return (uint32_t)k_ticks_to_us_floor64(k_uptime_ticks());
}

void midi_clock_set_step_callback(step_cb_t cb)
{
    step_callback = cb;
}

void midi_clock_poll(void)
{
    uint32_t now = now_us();
    
    if (master_clock_dev && master_clock_dev->running) {
        if ((int32_t)(now - master_next_tick_us) >= 0) {
            
            static uint32_t last_tick_us = 0;
            static uint32_t real_gap = 0;
            real_gap = now - last_tick_us;
            last_tick_us = now;
            
            char buf[32];
            snprintf(buf, sizeof(buf), "us:%u",real_gap);

            oled_clear_rect(0, 56, 128, 8);
            oled_write_small(buf, 0, 56, false);

            midi_clock(master_clock_dev->midi);
            
            uint32_t tick;
            ATOMIC(
                master_clock_dev->tick++;
                tick = master_clock_dev->tick;
            );
            
            master_next_tick_us = now + master_clock_dev->interval_us;

            if (tick % 96 == 0 && step_callback) {
                step_callback(tick / 96);
            }
        }
    }
    
    if (poly_clock_dev && poly_clock_dev->running) {
        if ((int32_t)(now - poly_next_tick_us) >= 0) {
            midi_clock(poly_clock_dev->midi);
            
            ATOMIC(
                poly_clock_dev->tick++;
            );
            
            poly_next_tick_us = now + poly_clock_dev->interval_us;
        }
    }
}

int midi_clock_init(midi_clock_dev_t *dev, midi_dev_t *midi, uint32_t bpm)
{
    if (!dev || !midi) return -EINVAL;

    dev->midi = midi;
    dev->interval_us = BPM_TO_US(bpm);
    dev->tick = 0;
    dev->running = false;

    if (master_clock_dev == NULL) {
        master_clock_dev = dev;
    } else if (poly_clock_dev == NULL) {
        poly_clock_dev = dev;
    }

    return 0;
}

void midi_clock_start(midi_clock_dev_t *dev)
{
    if (!dev || dev->running) return;

    dev->running = true;
    dev->tick = 0;

    if (dev == master_clock_dev) {
        midi_start(dev->midi);
        master_next_tick_us = now_us() + dev->interval_us;
    } else if (dev == poly_clock_dev) {
        poly_next_tick_us = now_us() + dev->interval_us;
    }
}

void midi_clock_stop(midi_clock_dev_t *dev)
{
    if (!dev || !dev->running) return;

    dev->running = false;

    if (dev == master_clock_dev) {
        midi_stop(dev->midi);
    }
}

void midi_clock_update(midi_clock_dev_t *dev, uint32_t bpm)
{
    if (!dev) return;
    uint32_t interval = BPM_TO_US(bpm);

    ATOMIC(dev->interval_us = BPM_TO_US(bpm));

    if (dev == master_clock_dev)
        master_next_tick_us = now_us() + interval;
}

void midi_clock_update_poly(midi_clock_dev_t *dev, uint32_t bpm, bool divide, uint8_t num)
{
    if (!dev) return;
    if (num==0) num = 16;

    uint32_t interval = divide ? POLY_BPM_TO_US_DIV(bpm, num)
                               : POLY_BPM_TO_US_MULT(bpm, num);

    ATOMIC(dev->interval_us = interval);

    if (dev == poly_clock_dev)
    poly_next_tick_us = now_us() + interval;
}

void midi_clock_sync_poly(midi_clock_dev_t *dev, uint32_t bpm, bool divide, uint8_t num)
{
    if (!dev || !dev->running) return;
    if (num == 0) num = 16;

    uint32_t interval = divide ? POLY_BPM_TO_US_DIV(bpm, num)
                               : POLY_BPM_TO_US_MULT(bpm, num);

    ATOMIC(
        dev->interval_us = interval;
        dev->tick        = 0;
    );

    midi_start(dev->midi);
    poly_next_tick_us = now_us() + interval;
}

// FOR FUTURE MIDI SYNC / NOT USED

// void midi_clock_set_mode(uint8_t mode)
// {
//     ATOMIC(clock_mode = mode);
// }

// void midi_clock_clock_me(void)
// {
//     uint32_t now = now_us();
//     uint32_t diff = 0;

//     if (ext_clock_us > 0) {
//         if (now >= ext_clock_us) {
//             diff = now - ext_clock_us;
//         } else {
//             diff = (0xFFFFFFFFUL - ext_clock_us) + now;
//         }
//     }

//     ext_clock_us = now;
//     ext_interval = diff;

//     if (ext_interval > 0 && ext_interval < 1000000) {
//         ext_interval_buffer[ext_interval_idx] = ext_interval;
//         ext_interval_idx = (ext_interval_idx + 1) % 64;
//         if (ext_interval_count < 64) ext_interval_count++;
//     }

//     uint64_t acc = 0;
//     uint8_t valid = 0;
//     for (uint8_t i = 0; i < ext_interval_count; i++) {
//         if (ext_interval_buffer[i] > 0) {
//             acc += ext_interval_buffer[i];
//             valid++;
//         }
//     }

//     if (valid > 0) {
//         float usecs = (float)acc / valid;
//         external_tempo = (60000000.0f / 24.0f) / usecs;
//         if (external_tempo < 1.0f) external_tempo = 1.0f;
//         if (external_tempo > 500.0f) external_tempo = 500.0f;
//     }
// }

// float midi_clock_get_tempo(void)
// {
//     if (clock_mode == UCLOCK_EXTERNAL) {
//         return external_tempo;
//     }
//     return 120.0f;
// }
