#ifndef DIGEE_UI_H
#define DIGEE_UI_H

#include "digee_ctrl.h"

/*
 * digee_ui.h - Presentation Layer
 *
 * Reads digee_state_t and composes the display. Never owns state,
 * never mutates digee_state_t, never touches the display device.
 *
 * Drawing order for a full redraw:
 *   1. ui_draw_full() — clears buffer and redraws all elements
 *
 * Or redraw individual regions when only one thing changes:
 *   ui_draw_bpm(), ui_draw_fraction(), etc.
 *
 * After any draw call, the caller is responsible for flushing
 * via display_hal_flush().
 */

/* ------------------------------------------------------------------ */
/* Layout constants                                                     */
/* ------------------------------------------------------------------ */

#define LOGO_X          6
#define LOGO_Y          20
#define LOGO_W          116
#define LOGO_H          25

#define PLAYSTATE_X     80
#define PLAYSTATE_Y     12
#define PLAYSTATE_W     10
#define PLAYSTATE_H     16

#define MATHSTATE_X     80
#define MATHSTATE_Y     38
#define MATHSTATE_W     10
#define MATHSTATE_H     16

#define FRACTION_X      48
#define FRACTION_X_SINGLE 53
#define FRACTION_TOP_Y  10
#define FRACTION_BOT_Y  37
#define FRACTION_W      20
#define FRACTION_H      16

#define BPM_X           11
#define BPM_X_QUAD      4
#define BPM_X_TRIPLE    12
#define BPM_X_DOUBLE    20
#define BPM_X_SINGLE    29
#define BPM_WIDTH       37
#define BPM_MAIN_Y      14
#define BPM_POLY_Y      46

#define DIGIT_X         112

/* ------------------------------------------------------------------ */
/* Boot sequence                                                        */
/* ------------------------------------------------------------------ */

/* Runs the boot animation then clears the buffer.
 * Blocks for the duration of the animation. */
void ui_boot(const char *version);

/* ------------------------------------------------------------------ */
/* Full redraw                                                          */
/* ------------------------------------------------------------------ */

/* Clears the buffer and redraws all UI elements from current state. */
void ui_draw_full(const digee_state_t *s);

/* ------------------------------------------------------------------ */
/* UI internal calc                                                     */
/* ------------------------------------------------------------------ */
uint8_t bpm_x_offset(uint16_t bpm);
uint16_t calc_ui_poly(const digee_state_t *s);

/* ------------------------------------------------------------------ */
/* Partial redraws                                                      */
/* ------------------------------------------------------------------ */
void ui_draw_bpm(const digee_state_t *s);
void ui_draw_fraction(const digee_state_t *s);
void ui_draw_playstate(const digee_state_t *s);
void ui_draw_mathstate(const digee_state_t *s);
void ui_draw_digits(const digee_state_t *s);


#endif /* DIGEE_UI_H */