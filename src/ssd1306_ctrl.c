#include "ssd1306_ctrl.h"
#include "font8x16.h"
#include "font5x7.h"

#include <stdint.h>
#include <string.h>

static oled_buf_t buf;

const oled_buf_t *oled_get_buffer() {
    return &buf;
}

// Could implement dirty rects concept but we need to discern what pages different elements of the display are in and need to be redrawn to make this efficient and worthwhile

// static dirty undrawnRects[MAX_RECTS] = {0};

// const dirty *getUndrawnRects() {
//     return &undrawnRects;
// }

// void append_dirty(uint8_t y,uint8_t h) {
//     int page = y / 8;
//     int end_page = (y + h - 1) / 8;

// }

void oled_clear() {
    memset(buf.data, 0x00, sizeof(buf.data));
}

void oled_fill() {
    memset(buf.data, 0xFF, sizeof(buf.data));
}

bool oled_get_pixel(uint8_t x, uint8_t y) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return false;
    return (buf.data[y / 8][x] >> (y % 8)) & 1;
}

void oled_set_pixel(uint8_t x, uint8_t y, bool on)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
 
    uint8_t page  = y / 8;
    uint8_t bit   = y % 8;
 
    if (on) {
        buf.data[page][x] |=  (1 << bit);
    } else {
        buf.data[page][x] &= ~(1 << bit);
    }
}

void oled_draw_bitmap(const uint8_t *bitmap, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (int i=0; i<h; i++) {
        for (int j=0; j<w; j++) {
            uint16_t byte_index = i* ((w + 7) / 8) + (j / 8); // which byte (array of 8) we are in
            uint8_t bit_index = j % 8; // modulo to find the bit within the byte
            bool state = (bitmap[byte_index] & (0x80 >> bit_index)) !=0;
            // oled_set_pixel(&buf, x+j, y+i, state);
            oled_set_pixel(x+j, y+i, state);
        }
    }
}

void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (int i=0; i<h; i++) {
        for (int j=0; j<w; j++) {
            oled_set_pixel(x+j, y+i, true);
        }
    }
}

void oled_clear_rect( uint8_t x, uint8_t y,
                                    uint8_t width, uint8_t height) {
    for (int i=0; i<height; i++) {
        for (int j=0; j<width; j++) {
            oled_set_pixel(x+j, y+i, false);
        }
    }
}

void oled_draw_char(char c, uint8_t x, uint8_t y, bool inverted) {
    const roboto_glyph_t *glyph = font_roboto_get(c);
 
    for (int row = 0; row < FONT_ROBOTO_HEIGHT; row++) {
        uint8_t row_data = glyph->data[row];
        if (inverted) {
            row_data = ~row_data;
        }
        for (int col = 0; col < FONT_ROBOTO_WIDTH; col++) {
            bool state = (row_data & (0x80 >> col)) != 0;
            oled_set_pixel(x + col, y + row, state);
        }
    }
}

void oled_draw_char_small(char c, uint8_t x, uint8_t y, bool inverted) {
    //DRAW IN COLUMNS VERTICALLY NOT ROWS
    for (int column=0; column < FONT5X7_WIDTH; column++) {
        uint8_t columnData = font5x7[c - 32][column];  
        if (inverted) {columnData =~ columnData;}    
        for (int row=0; row< FONT5X7_HEIGHT; row++) {
            bool state = (columnData & (1 << row)) !=0;
            oled_set_pixel(x+column,  y+row, state);
        }
    }
}

void oled_write(const char *str, uint8_t x, uint8_t y, bool inverted) {
    while (*str) {
        if (x + FONT_ROBOTO_WIDTH > OLED_WIDTH) {
            break;
        }
        oled_draw_char(*str++, x, y, inverted);
        x += FONT_ROBOTO_WIDTH + 1;
    }
}

void oled_write_small(const char *string, uint8_t x, uint8_t y, bool inverted) {
    while (*string !=0) {
        oled_draw_char_small(*string, x, y, inverted);
        string++;
        x+=FONT5X7_WIDTH+1;
    }
}