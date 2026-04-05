#ifndef SSD1306_CTRL_H
#define SSD1306_CTRL_H

#include <stdint.h>
#include <stdbool.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES  (OLED_HEIGHT / 8)

/* The framebuffer */
typedef struct {
    uint8_t data[OLED_PAGES][OLED_WIDTH];
} oled_buf_t;

// // Dirty rects to draw
// typedef struct {
//     uint8_t x;
//     uint8_t y;
//     uint8_t w;
//     uint8_t h;
// } dirty;

// #define MAX_RECTS 11

// const dirty *getUndrawnRects(void);

const oled_buf_t *oled_get_buffer(void);


void oled_clear(void);
void oled_fill(void);

void oled_set_pixel(uint8_t x, uint8_t y, bool on);
bool oled_get_pixel(uint8_t x, uint8_t y);

void oled_draw_bitmap(const uint8_t *bitmap, uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void oled_clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

void oled_draw_char(char c, uint8_t x, uint8_t y, bool inverted);
void oled_write(const char *str, uint8_t x, uint8_t y, bool inverted);

void oled_draw_char_small(char c, uint8_t x, uint8_t y, bool inverted);
void oled_write_small(const char *str, uint8_t x, uint8_t y, bool inverted);

#endif