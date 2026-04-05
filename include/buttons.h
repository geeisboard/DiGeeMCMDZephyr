#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

typedef enum {
    BUTTON_1 = 0,
    BUTTON_2,
    BUTTON_4,
    BUTTON_8,
    BUTTON_COUNT
} button_id_t;

typedef enum {
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
} button_evt_t;

typedef void (*button_cb_t)(button_id_t id, button_evt_t);

int buttons_init(button_cb_t);

#endif