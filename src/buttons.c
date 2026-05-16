#include "buttons.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buttons, LOG_LEVEL_INF);

#define DEBOUNCE_MS 20

/* ------------------------------------------------------------------ */
/* Per-button descriptor — table driven so adding buttons = one line  */
/* ------------------------------------------------------------------ */

struct button_desc {
    const struct gpio_dt_spec spec;  /* pin + flags from devicetree  */
    struct gpio_callback      cb_data;
    struct k_work_delayable   work;
    button_id_t               id;
    button_evt_t              last_evt;
};

/* DT_NODELABEL pulls the exact spec from your overlay at compile time */
static struct button_desc s_buttons[BUTTON_COUNT] = {
    { .spec = GPIO_DT_SPEC_GET(DT_NODELABEL(button0), gpios), .id = BUTTON_1 },
    { .spec = GPIO_DT_SPEC_GET(DT_NODELABEL(button1), gpios), .id = BUTTON_2 },
    { .spec = GPIO_DT_SPEC_GET(DT_NODELABEL(button2), gpios), .id = BUTTON_4 },
    { .spec = GPIO_DT_SPEC_GET(DT_NODELABEL(button3), gpios), .id = BUTTON_8 },
};

static button_cb_t s_cb;

static void button_work_handler(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct button_desc *btn = CONTAINER_OF(dwork, struct button_desc, work);

    int val = gpio_pin_get_dt(&btn->spec);
    if (val == 0) {
        s_cb(btn->id, BUTTON_EVT_RELEASED);
    }
    /* no else — ignore the press edge entirely */
}

static void button_isr(const struct device *port,
                       struct gpio_callback *cb_data,
                       gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    struct button_desc *btn = CONTAINER_OF(cb_data, struct button_desc, cb_data);
    k_work_reschedule(&btn->work, K_MSEC(DEBOUNCE_MS));
    
}

// PUBLIC

int buttons_init(button_cb_t cb)
{
    if (!cb) return -EINVAL;
    s_cb = cb;

    /* Pass 1 — configure all pins as inputs, no interrupts yet */
    for (int i = 0; i < BUTTON_COUNT; i++) {
        struct button_desc *btn = &s_buttons[i];

        if (!gpio_is_ready_dt(&btn->spec)) {
            LOG_ERR("Button %d GPIO not ready", i);
            return -ENODEV;
        }

        /* Disable interrupt first — clears any leftover state from reset */
        gpio_pin_interrupt_configure_dt(&btn->spec, GPIO_INT_DISABLE);

        int setup_failed = gpio_pin_configure_dt(&btn->spec, GPIO_INPUT);
        if (setup_failed) {
            LOG_ERR("Button %d configure failed (%d)", i, setup_failed);
            return setup_failed;
        }

        k_work_init_delayable(&btn->work, button_work_handler);
        gpio_init_callback(&btn->cb_data, button_isr, BIT(btn->spec.pin));
        gpio_add_callback(btn->spec.port, &btn->cb_data);
    }

    /* Let pins settle before enabling interrupts */
    k_msleep(100);

    /* Pass 2 — enable interrupts now that pins are stable */
    for (int i = 0; i < BUTTON_COUNT; i++) {
        struct button_desc *btn = &s_buttons[i];

        int setup_failed = gpio_pin_interrupt_configure_dt(&btn->spec, GPIO_INT_EDGE_BOTH);
        if (setup_failed) {
            LOG_ERR("Button %d interrupt enable failed (%d)", i, setup_failed);
            return setup_failed;
        }

        LOG_INF("Button %d ready on pin %d", i, btn->spec.pin);
    }

    return 0;
}