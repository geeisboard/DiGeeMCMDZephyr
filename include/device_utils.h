#ifndef DEVICE_UTILS_H
#define DEVICE_UTILS_H

#include <zephyr/device.h>

#define DEVICE_INIT_CHECK(dev, name)             \
    do {                                         \
        if (!device_is_ready(dev)) {             \
            LOG_ERR(name " device not ready");   \
            return -ENODEV;                      \
        }                                        \
        LOG_INF(name " device ready");           \
    } while (0)

#endif
