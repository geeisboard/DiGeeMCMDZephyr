#include "clocks.h"
#include "midi.h"
#include <stdint.h>
// #include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>
#include <zephyr/spinlock.h>

static struct k_spinlock _lock;   // Spinlock

#define ATOMIC(x) do { \
    k_spinlock_key_t _key = k_spin_lock(&_lock); \
    x; \
    k_spin_unlock(&_lock, _key); \
} while (0)

static midi_clock_dev_t *m_clock_dev = NULL; // Main clock pointer
static midi_clock_dev_t *p_clock_dev = NULL; // Poly clock pointer
static step_cb_t step_callback = NULL; // Loop callback
static clock_controller_t *clock_ctrl = NULL; // Internal struct that retains clock information

// FOR FUTURE SYNCHRONISATION WITH EXTERNAL CLOCK
// static volatile uint8_t clock_mode = UCLOCK_INTERNAL;
// static volatile uint32_t ext_clock_us = 0;
// static volatile uint32_t ext_interval = 0;
// static volatile float external_tempo = 120.0f;
// static uint32_t ext_interval_buffer[64] = {0};
// static uint8_t ext_interval_idx = 0;
// static uint8_t ext_interval_count = 0;

static inline uint32_t now_us(void)
{
    return (uint32_t)k_ticks_to_us_floor64(k_uptime_ticks());
}

uint32_t POLY_BPM_TO_US(clock_controller_t *dev)
{   
    uint8_t num = dev->binary;
    if (num==0) num = 16;
    if (dev->divide_s) {
        printf("dividing to get interval\n");
        return POLY_BPM_TO_US_DIV(dev->bpm, num);
    } else {
        printf("multiplying to get interval\n");
        return POLY_BPM_TO_US_MULT(dev->bpm, num);
    }
}

void clock_set_step_callback(step_cb_t cb)
{
    step_callback = cb;
}

void midi_clock_poll(void)
{
    // GET CURRENT TIME
    uint32_t now = now_us();
    // MASTER CLOCK FIRST. IF RUNNING AND ITS NOT NULL >>
    if (m_clock_dev && m_clock_dev->running) {
        // IF THE CURRENT TIME IS GREATER THAN THE NEXT DECLARED TICK TIME
        // CAST RESULT TO SIGNED INTEGER FOR SUBTRACTION
        if ((int32_t)(now - m_clock_dev->next_tick_us) >= 0) {
            // SEND MIDI CLOCK MESSAGE
            midi_clock(m_clock_dev->midi);     
            // SET NEXT INTERVAL TIME
            m_clock_dev->next_tick_us = now + m_clock_dev->interval_us;
            // UPDATE TICK
            ATOMIC(m_clock_dev->tick++);
            // A bit cheeky here, but it sounds natural in practice so let it be xx
            if (m_clock_dev->tick >= 96 && step_callback) {
                m_clock_dev->tick = 0;
                step_callback(m_clock_dev->tick);
            }
        }
    }
    
    if (p_clock_dev && p_clock_dev->running) {
        if ((int32_t)(now - p_clock_dev->next_tick_us) >= 0) {
            // SEND MIDI CLOCK MESSAGE
            midi_clock(p_clock_dev->midi);
            // SET NEXT INTERVAL TIME
            p_clock_dev->next_tick_us = now + p_clock_dev->interval_us;
        }
    }
}

int clock_ctrl_init(clock_controller_t *dev) 
{    
    if (!dev) return -EINVAL;
    // Assign clock values from Digee_ctrl to internal clock values
    clock_ctrl = dev;
    return 0;
}

int clock_init(midi_clock_dev_t *dev, midi_dev_t *midi, clock_type_t type)
{
    if (!dev || !midi) return -EINVAL;
    // Init clock
    dev->midi = midi;
    dev->tick = 0;
    dev->running = false;
    dev->next_tick_us =0;

    // MASTER
    if (type == MASTER_CLOCK) {
        dev->interval_us = BPM_TO_US(clock_ctrl->bpm);
        m_clock_dev = dev;
    } else if (type == POLY_CLOCK) {
        dev->interval_us = POLY_BPM_TO_US(clock_ctrl);
        p_clock_dev = dev; 
    }

    return 0;
}

void clock_start(clock_controller_t *dev) 
{
    if (!dev || dev->paused) return;
    
    // Update clock_ctrl values
    clock_ctrl = dev;
    // IF THIS RUNS ITS A RESYNC INSTEAD BECAUSE 
    if (m_clock_dev->running) {
        uint32_t new_interval_p = POLY_BPM_TO_US(clock_ctrl);
        p_clock_dev->tick = 0;
        p_clock_dev->next_tick_us = now_us() + POLY_BPM_TO_US(clock_ctrl);
        ATOMIC(p_clock_dev->interval_us = new_interval_p); // Use ATOMIC since we are live
        midi_start(m_clock_dev->midi);
        midi_start(p_clock_dev->midi);
        return;
    }
    // Set clocks to run
    m_clock_dev->running = true;
    p_clock_dev->running = true;
    // Calc new intervals
    uint32_t new_interval_m = BPM_TO_US(clock_ctrl->bpm);
    uint32_t new_interval_p = POLY_BPM_TO_US(clock_ctrl);
    // Reset master tick to 0, we dont increment the poly tick, no need
    m_clock_dev->tick = 0;
    // Assign next tick time
    m_clock_dev->next_tick_us = now_us() + BPM_TO_US(clock_ctrl->bpm);
    p_clock_dev->next_tick_us = now_us() + POLY_BPM_TO_US(clock_ctrl);
    // Assign new interval between ticks, no need for ATOMIC since we are starting the clocks??
    m_clock_dev->interval_us = new_interval_m;
    p_clock_dev->interval_us = new_interval_p;
    // Call midi start to both devices
    midi_start(m_clock_dev->midi);
    midi_start(p_clock_dev->midi);
}

void clock_stop(clock_controller_t *dev)
{
    if (!dev || !dev->paused) return;
    // Update clock_ctrl values
    clock_ctrl = dev;
    // Change clock states
    m_clock_dev->running = false;
    p_clock_dev->running = false;
    // Call midi stop to both devices
    midi_stop(m_clock_dev->midi);
    midi_stop(p_clock_dev->midi);
}

void clock_update(clock_controller_t *dev)
{
    if (!dev) return;
    // Update clock_ctrl values
    clock_ctrl = dev;
    // Calc new intervals
    uint32_t new_interval_m = BPM_TO_US(clock_ctrl->bpm);
    uint32_t new_interval_p = POLY_BPM_TO_US(clock_ctrl);
    // Update new intervals with ATOMIC since we are live
    ATOMIC(m_clock_dev->interval_us = new_interval_m);
    ATOMIC(p_clock_dev->interval_us = new_interval_p);
    // Get current time
    uint32_t now = now_us();
    // Only update the next tick time IF the new interval time + now is sooner than the old tick + now
    if ((int32_t)(m_clock_dev->next_tick_us - (now + new_interval_m)) > 0)
        m_clock_dev->next_tick_us = now + new_interval_m;
    // Only update the next tick time IF the new interval time + now is sooner than the old tick + now
    if ((int32_t)(p_clock_dev->next_tick_us - (now + new_interval_p)) > 0)
        p_clock_dev->next_tick_us = now + new_interval_p;
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
