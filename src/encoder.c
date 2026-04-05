#include "encoder.h"
#include "zephyr/sys/util.h"
#include "zephyr/toolchain.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(encoder, LOG_LEVEL_INF);

#define DEBOUNCE_MS 30
/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

static const struct device *s_enc;
static encoder_rt_cb_t      s_rt_cb;
static int32_t              s_detent;
static int32_t              s_prev_pos;
static int32_t              s_accum;

static encoder_btn_cb_t     s_btn_cb;

struct encoder_btn_t {
    const struct gpio_dt_spec spec;  /* pin + flags from devicetree  */
    struct gpio_callback      cb_data;
    struct k_work_delayable   work;
    enc_button_evt_t          last_evt;
    struct k_timer            dbl_delay;
};

static struct encoder_btn_t enc_btn = {
    .spec = GPIO_DT_SPEC_GET(DT_NODELABEL(encodersw0), gpios),
    .last_evt = ENC_BUTTON_EVT_NONE
};


/* ------------------------------------------------------------------ */
/* Work item — runs in system workqueue, safe for I2C / logging       */
/* ------------------------------------------------------------------ */

static void enc_btn_timer_cb (struct k_timer *timer)
{   
    // Timer function triggered after 30 ms
    struct encoder_btn_t *btn = CONTAINER_OF(timer, struct encoder_btn_t, dbl_delay);
    btn->last_evt = ENC_BUTTON_EVT_NONE;
    s_btn_cb(ENC_BUTTON_EVT_RELEASED);
}

static void enc_btn_isr (const struct device *port, struct gpio_callback *cb_data, gpio_port_pins_t pins) 
{
    ARG_UNUSED(pins);
    ARG_UNUSED(port);
    struct encoder_btn_t *btn = CONTAINER_OF(cb_data, struct encoder_btn_t, cb_data);
    k_work_reschedule(&btn->work, K_MSEC(DEBOUNCE_MS));
};

static void enc_btn_work_handler(struct k_work *work) 
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct encoder_btn_t *btn = CONTAINER_OF(dwork, struct encoder_btn_t, work);

    int val = gpio_pin_get_dt(&btn->spec);

    if (val==0 && btn->last_evt==ENC_BUTTON_EVT_RELEASED) {
        k_timer_stop(&btn->dbl_delay);
        btn->last_evt = ENC_BUTTON_EVT_NONE;
        s_btn_cb(ENC_BUTTON_EVT_DOUBLE_PRESS);
    } else if (val==0) {
        // First release, start timer but do not trigger callback
        // s_btn_cb(ENC_BUTTON_EVT_RELEASED);
        btn->last_evt = ENC_BUTTON_EVT_RELEASED;
        k_timer_start(&btn->dbl_delay, K_MSEC(300), K_NO_WAIT);
    }
};

int encoder_btn_init(encoder_btn_cb_t cb) 
{
    if (!cb) return -EINVAL;
    s_btn_cb = cb;

    struct encoder_btn_t *btn = &enc_btn;

    if (!gpio_is_ready_dt(&btn->spec)) {
        LOG_ERR("Encoder button not ready");
        return -ENODEV;
    }

    // Clear interrupt previous states on reset first
    gpio_pin_interrupt_configure_dt(&btn->spec, GPIO_INT_DISABLE);

    int ret = gpio_pin_configure_dt(&btn->spec, GPIO_INPUT);

    if (ret) {
        LOG_ERR("Encoder button configure failed");
        return ret;
    }
    k_timer_init(&btn->dbl_delay, enc_btn_timer_cb, NULL);
    k_work_init_delayable(&btn->work, enc_btn_work_handler);
    gpio_init_callback(&btn->cb_data, enc_btn_isr, BIT(btn->spec.pin));
    gpio_add_callback(btn->spec.port, &btn->cb_data);

    k_msleep(100);

    ret = gpio_pin_interrupt_configure_dt(&btn->spec, GPIO_INT_EDGE_BOTH);

    if (ret) {
        LOG_ERR("Encoder button interrupt enable failed ");
        return ret;
    }

    return 0;
};

/* ------------------------------------------------------------------ */
/* Polling fallback thread — used when trigger not supported          */
/* ------------------------------------------------------------------ */

#define ENCODER_POLL_MS  10
#define ENCODER_STACK_SZ 512
#define ENCODER_PRIORITY -1

K_THREAD_STACK_DEFINE(encoder_poll_stack, ENCODER_STACK_SZ);
static struct k_thread enc_poll_thread_data;

static void enc_poll_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    while (1) {
        struct sensor_value val;
        sensor_sample_fetch(s_enc);
        sensor_channel_get(s_enc, SENSOR_CHAN_ROTATION, &val);

        int32_t pos   = val.val1;
        int32_t delta = pos - s_prev_pos;
        s_prev_pos    = pos;
        s_accum      += delta;

        if (s_accum >= s_detent) {
            int32_t steps = s_accum / s_detent;
            s_rt_cb(steps);
            s_accum %= s_detent;
        } else if (s_accum <= -s_detent) {
            int32_t steps = s_accum / s_detent;
            s_rt_cb(steps);
            s_accum %= s_detent;
        }

        k_msleep(ENCODER_POLL_MS);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int encoder_rt_init(encoder_rt_cb_t cb, int32_t detent)
{
    if (!cb || detent <= 0) return -EINVAL;

    s_enc = DEVICE_DT_GET(DT_ALIAS(qdec0));
    if (!device_is_ready(s_enc)) {
        LOG_ERR("Encoder device not ready");
        return -ENODEV;
    }

    LOG_INF("Encoder device ready");

    s_rt_cb       = cb;
    s_detent   = detent;
    s_accum    = 0;
    // s_use_polling = false;

    /* Settle before reading initial position */
    k_msleep(50);

    struct sensor_value val;
    sensor_sample_fetch(s_enc);
    sensor_channel_get(s_enc, SENSOR_CHAN_ROTATION, &val);
    s_prev_pos = val.val1;

    // Always use polling — no ISR
    k_thread_create(&enc_poll_thread_data,
                    encoder_poll_stack,
                    K_THREAD_STACK_SIZEOF(encoder_poll_stack),
                    enc_poll_thread,
                    NULL, NULL, NULL,
                    ENCODER_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&enc_poll_thread_data, "enc_poll");

    LOG_INF("Encoder ready — polling at %dms (detent=%d)", ENCODER_POLL_MS, detent);
    return 0;
}