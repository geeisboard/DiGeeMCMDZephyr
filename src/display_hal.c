#include "display_hal.h"
#include "ssd1306_ctrl.h"

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(display_hal, LOG_LEVEL_ERR);

static const struct device *display_dev = DEVICE_DT_GET(DT_NODELABEL(ssd1306));

int display_hal_init(void)
{
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    display_blanking_off(display_dev);

    return 0;
}

int display_hal_flush(void)
{
    const oled_buf_t *buf = oled_get_buffer();

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(buf->data),
        .width    = OLED_WIDTH,
        .height   = OLED_HEIGHT,
        .pitch    = OLED_WIDTH,
    };

    return display_write(display_dev, 0, 0, &desc, buf->data);
}