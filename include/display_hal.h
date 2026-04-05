#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H
 
#include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
// #include <stdbool.h> // do we even need this

int display_hal_init(void);

int display_hal_flush(void);

int display_hal_print(uint16_t x, uint16_t y, const char *str);
void display_hal_clear_text(void);
 
#endif /* DISPLAY_HAL_H */