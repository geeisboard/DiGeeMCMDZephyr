#ifndef DIGEE_CTRL_H
#define  DIGEE_CTRL_H

#include <stdint.h>
#include <stdbool.h>

#define DIGEE_FIXED 16
#define BINARY_ONE      0
#define BINARY_TWO      1
#define BINARY_FOUR     2
#define BINARY_EIGHT    3

typedef struct {
    uint8_t     bpm_main;
    // uint16_t    bpm_poly;
    uint8_t     binary;
    uint8_t     binary_prev;
    bool        divide_state;
    bool        pause_state;
    bool        equation_flag;
    bool        invert;
} digee_state_t;

extern volatile bool ui_dirty;

/* Initialise device state and hardware. Call once at boot. */
void digee_init(void);

/* Main update tick — call from application thread or main loop. */
void digee_update(void);

void digee_ui_update(void);

/* ------------------------------------------------------------------ */
/* State accessors                                                      */
/* ------------------------------------------------------------------ */

/* Returns a read-only pointer to current device state.
 * UI layer uses this to read values for rendering. */
const digee_state_t *digee_get_state(void);

/* ------------------------------------------------------------------ */
/* State mutators — only digee_ctrl changes state                      */
/* ------------------------------------------------------------------ */
void digee_update_clocks();
void digee_increment_bpm(int8_t by);
void digee_set_bpm_poly(uint16_t bpm);
void digee_set_custom(uint8_t custom);
void digee_change_playstate(void);
void digee_change_dividestate(void);

void digee_toggle_pause(void);
void digee_toggle_divide(void);
void digee_toggle_invert(void);
void digee_set_equation_flag(bool value);

#endif /* DIGEE_CTRL_H */