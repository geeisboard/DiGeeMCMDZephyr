#ifndef I2C_UTILS_H
#define I2C_UTILS_H

#include <zephyr/drivers/i2c.h>

#define I2C_SCAN(i2c_dev)                                                   \
do {                                                                        \
    LOG_INF("Scanning I2C bus...");                                         \
    uint8_t _found = 0;                                                     \
    for (uint8_t _addr = 0x08; _addr < 0x78; _addr++) {                    \
        struct i2c_msg _msg = {                                             \
            .buf   = &(uint8_t){0},                                         \
            .len   = 0,                                                     \
            .flags = I2C_MSG_WRITE | I2C_MSG_STOP,                         \
        };                                                                  \
        if (i2c_transfer(i2c_dev, &_msg, 1, _addr) == 0) {                 \
            LOG_INF("  Found: 0x%02x", _addr);                             \
            _found++;                                                       \
        }                                                                   \
    }                                                                       \
    if (_found == 0) {                                                      \
        LOG_ERR("No I2C devices found - check wiring");                    \
        return -ENODEV;                                                     \
    }                                                                       \
    LOG_INF("Scan complete, %d device(s) found", _found);                  \
} while (0)

#endif