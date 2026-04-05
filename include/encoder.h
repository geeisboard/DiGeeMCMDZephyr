#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

typedef void (*encoder_rt_cb_t)(int32_t delta);

/**
 * @param cb      Called with +1 or -1 on each detent step.
 * @param detent  Raw counts per physical click (e.g. 18).
 * @return 0 on success, negative errno on failure.
 */
int encoder_rt_init(encoder_rt_cb_t cb, int32_t detent);

typedef enum {
    ENC_BUTTON_EVT_NONE,
    ENC_BUTTON_EVT_DOUBLE_PRESS,
    ENC_BUTTON_EVT_RELEASED
} enc_button_evt_t;

typedef void (*encoder_btn_cb_t)(enc_button_evt_t);

int encoder_btn_init(encoder_btn_cb_t cb);

#endif /* ENCODER_H */