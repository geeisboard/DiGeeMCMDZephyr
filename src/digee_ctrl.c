#include "digee_ctrl.h"
#include "clocks.h"
#include "digee_ui.h"
#include "encoder.h"  
#include "buttons.h"
#include "display_hal.h"
#include "midi.h"
#include "zephyr/devicetree.h"
#include "zephyr/kernel.h"
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/timing/timing.h>

#define BPM_MAX 240
#define BPM_MIN 40
#define ENC_DETENT 18

#define START_BPM 120
#define START_BINARY 0 // 0 representing 16 btw lol
#define START_DIVIDE true
#define START_PAUSE true

static const char version [] = "v1.0.0";
static volatile int32_t encoder_delta = 0;

static midi_dev_t   midi_master;
static midi_dev_t   midi_poly;
static midi_clock_dev_t master_clock;
static midi_clock_dev_t poly_clock;

volatile bool ui_dirty = false;

digee_state_t digee_state = {
    .binary_ui = START_BINARY, // Visual record of the binary, updates on screen immediately, must remain 0 when it represents 16 as we write to it directly with bit shifting
    .divide_s_ui = true,
    .invert = false,
    .clock = {
        .binary = START_BINARY, // Clock record of the binary, updates on loop
        .bpm = START_BPM, // Clock bpm
        .divide_s = START_DIVIDE,
        .paused = START_PAUSE
    }
};

void digee_toggle_binary_bit(button_id_t btn)
{
    // Bit shift the number to the UI Binary
    digee_state.binary_ui ^= (1U << btn);
    // If paused we can update prev value immediately
    if (digee_state.clock.paused) {
        digee_state.clock.binary = digee_state.binary_ui;
    }
}

static void on_button_event(button_id_t id, button_evt_t evt)
{
    if (evt != BUTTON_EVT_RELEASED) return;
    ui_dirty = true;
    // TURN BACK ON FOR FINAL VERSION
    // digee_toggle_binary_bit(id);

    // DEBUGGING SWITCH CASE WITH BUTTONS UNTIL WE HAVE SOLID ROTARY ENCODER DEBOUNCING
    switch (id) {
        case BUTTON_8:
            digee_change_playstate();
            break;
        case BUTTON_4:
            digee_change_dividestate();
            break;
        case BUTTON_2:
            digee_toggle_binary_bit(BUTTON_8);
            break;
        case BUTTON_1:
            digee_toggle_binary_bit(BUTTON_4);
            break;
        default:
        break;
    } 
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
        digee_change_playstate();
    }
    ui_dirty = true;
}

static void on_96step_callback(uint8_t step)
{   
    // Only trigger clock calculation if the binary has changed, OR the division state has changed
    if (digee_state.binary_ui == digee_state.clock.binary && digee_state.divide_s_ui == digee_state.clock.divide_s) return;
    // Move UI changes to the clock
    digee_state.clock.divide_s = digee_state.divide_s_ui;
    digee_state.clock.binary = digee_state.binary_ui;
    // printf("96 ticks completed, triggering sync\n");
    // Start clocks again with new timing
    clock_start(&digee_state.clock);
}

/* Main update tick — call from application thread or main loop. */
void digee_update() 
{
    int32_t delta = encoder_delta;
    
    encoder_delta = 0;
    if (delta != 0) {
        // printf("checking bpm incrementation: %i\n", delta);
        digee_state.clock.bpm += delta; // Increment bpm immediately
        // Constrain bpm to within MIN/MAX ranges
        if (digee_state.clock.bpm > BPM_MAX) digee_state.clock.bpm = BPM_MAX;
        if (digee_state.clock.bpm < BPM_MIN) digee_state.clock.bpm = BPM_MIN;
        digee_update_clocks();   // Update clocks with new bpm
        ui_dirty = true;       
    }
}

/* Initialise device state and hardware. Call once at boot. */
void digee_init(void) 
{
    k_msleep(200);
    display_hal_init();
    ui_boot(version);

    // ASSIGN PORTS > INIT CLOCK INFO > MIDI DEVICES > INIT CLOCKS > ASSIGN 96 STEP LOOP CALLBACK
    const struct device *uart_master = DEVICE_DT_GET(DT_ALIAS(midi_master_uart));
    const struct device *uart_poly = DEVICE_DT_GET(DT_ALIAS(midi_poly_uart));
    clock_ctrl_init(&digee_state.clock);
    midi_init(&midi_master, uart_master);
    midi_init(&midi_poly, uart_poly);
    midi_clock_init(&master_clock, &midi_master, MASTER_CLOCK);
    midi_clock_init(&poly_clock, &midi_poly, POLY_CLOCK);
    midi_clock_set_step_callback(on_96step_callback);
    // Init sensors like buttons and encoder
    buttons_init(on_button_event);
    encoder_btn_init(on_encoder_push);
    encoder_rt_init(on_encoder_step, ENC_DETENT);
    digee_ui_update(); // Draw default values to screen
}

void digee_ui_update() 
{
    ui_draw_full(&digee_state);
}

void digee_change_playstate()
{
    // Flip clock state
    digee_state.clock.paused = !digee_state.clock.paused;

    if (digee_state.clock.paused) {
        // STOP CLOCKS
        // When pausing, assign clock binary the UI binary since we're not waiting for a 96 step synchronisation
        digee_state.clock.binary = digee_state.binary_ui;
        clock_stop(&digee_state.clock);
    } else {
        // START CLOCKS
        clock_start(&digee_state.clock);
    }
}

void digee_update_clocks() 
{
    clock_update(&digee_state.clock);
}

void digee_change_dividestate()
{
    // digee_state.equation_flag = true;
    digee_state.divide_s_ui = !digee_state.divide_s_ui;
    ui_dirty = true;
}

/* ------------------------------------------------------------------ */
/* State accessors                                                      */
/* ------------------------------------------------------------------ */

/* Returns a read-only pointer to current device state.
 * UI layer uses this to read values for rendering. */
const digee_state_t *digee_get_state() {
    return &digee_state;
}