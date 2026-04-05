#include "digee_ctrl.h"
#include "digee_ui.h"
#include "encoder.h"  
#include "buttons.h"
#include "clocks.h"
#include "display_hal.h"
#include "midi.h"
#include "zephyr/devicetree.h"
#include "zephyr/kernel.h"
#include <stdbool.h>
#include <stdint.h>
#include "ssd1306_ctrl.h"

#define BPM_MAX 240
#define BPM_MIN 40
#define ENC_DETENT 18

static const char version [] = "v1.0.0";
static volatile int32_t encoder_delta = 0;

static midi_dev_t   midi_master;
static midi_dev_t   midi_poly;
static midi_clock_dev_t master_clock;
static midi_clock_dev_t poly_clock;

volatile bool ui_dirty = false;

digee_state_t digee_state = {
    .bpm_main = 120,
    // .bpm_poly = 120,
    .binary = 0,
    .binary_prev = 0,
    .divide_state = true,
    .pause_state = true,
    .equation_flag = false,
    .invert = false,
};

void digee_toggle_binary_bit(button_id_t btn)
{
    digee_state.binary ^= (1U << btn);
}

static void on_button_event(button_id_t id, button_evt_t evt)
{
    if (evt != BUTTON_EVT_RELEASED) return;

    digee_toggle_binary_bit(id);
    digee_change_playstate();
    ui_dirty = true;
}

static void on_encoder_step(int32_t delta)
{
    encoder_delta += delta;
}

static void on_encoder_push(enc_button_evt_t evt)
{
    if (evt == ENC_BUTTON_EVT_DOUBLE_PRESS) {
        digee_change_dividestate();
    } else if (evt == ENC_BUTTON_EVT_RELEASED) {
        // digee_change_playstate();
    }
}

static void on_step_callback(uint32_t step)
{   
    if (digee_state.binary == digee_state.binary_prev) return;

    digee_state.binary_prev = digee_state.binary;
    
    midi_clock_sync_poly(&poly_clock,
                         digee_state.bpm_main,
                         digee_state.divide_state,
                         digee_state.binary);

}

/* Main update tick — call from application thread or main loop. */
void digee_update() 
{
    int32_t delta = encoder_delta;

    char buf[7];
    snprintf(buf, sizeof(buf), "%d", encoder_delta);

    oled_clear_rect(0, 49, 60, 8);
    oled_write_small(buf, 0, 49, false);

    encoder_delta = 0;
    if (delta != 0) {
        digee_state.bpm_main += delta;
        if (digee_state.bpm_main > BPM_MAX) digee_state.bpm_main = BPM_MAX;
        if (digee_state.bpm_main < BPM_MIN) digee_state.bpm_main = BPM_MIN;

        digee_update_clocks();   // only called when BPM actually changed
        ui_dirty = true;       
    }

}

/* Initialise device state and hardware. Call once at boot. */
void digee_init(void) 
{
    k_msleep(200);
    display_hal_init();
    ui_boot(version);

    const struct device *uart_master = DEVICE_DT_GET(DT_ALIAS(midi_master_uart));
    const struct device *uart_poly = DEVICE_DT_GET(DT_ALIAS(midi_poly_uart));

    midi_init(&midi_master, uart_master);
    midi_init(&midi_poly, uart_poly);

    midi_clock_init(&master_clock, &midi_master, digee_state.bpm_main);
    midi_clock_init(&poly_clock, &midi_poly, digee_state.bpm_main);

    midi_clock_set_step_callback(on_step_callback);

    buttons_init(on_button_event);
    encoder_btn_init(on_encoder_push);
    encoder_rt_init(on_encoder_step, ENC_DETENT);
    digee_ui_update();
}

void digee_ui_update() 
{
    ui_draw_full(&digee_state);
}

void digee_change_playstate()
{
    digee_state.pause_state = !digee_state.pause_state;
    if (digee_state.pause_state) {
        midi_clock_stop(&master_clock);
        midi_clock_stop(&poly_clock);
    } else {
        midi_clock_start(&master_clock);
        midi_clock_start(&poly_clock);
    }
}

void digee_update_clocks(void) 
{
    midi_clock_update(&master_clock, digee_state.bpm_main);
    midi_clock_update_poly(&poly_clock, digee_state.bpm_main, digee_state.divide_state, digee_state.binary);
}

void digee_change_dividestate()
{
    digee_state.divide_state = !digee_state.divide_state;
    ui_dirty = true;
}

void digee_increment_bpm(int8_t by) 
{
    digee_state.bpm_main += by;
    if (digee_state.bpm_main > BPM_MAX) digee_state.bpm_main = BPM_MAX;
    if (digee_state.bpm_main < BPM_MIN) digee_state.bpm_main = BPM_MIN;
}

/* ------------------------------------------------------------------ */
/* State accessors                                                      */
/* ------------------------------------------------------------------ */

/* Returns a read-only pointer to current device state.
 * UI layer uses this to read values for rendering. */
const digee_state_t *digee_get_state() {
    return &digee_state;
}