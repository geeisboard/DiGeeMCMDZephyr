#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include "digee_ctrl.h"
#include "clocks.h"
#include <zephyr/drivers/sensor.h>


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define I2C_NODE DT_NODELABEL(i2c1)

int main(void)
{   
    k_msleep(2000); /* wait for serial monitor to connect */
    LOG_INF("Application started");

    digee_init();

    while (1) {
        midi_clock_poll();
        
        digee_update();
        
        if (ui_dirty) {
            ui_dirty = false;
            digee_ui_update();
        }
    }

    return 0;
}