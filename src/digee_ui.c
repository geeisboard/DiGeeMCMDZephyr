#include "digee_ui.h"
#include "digee_ctrl.h"
#include "ssd1306_ctrl.h"
#include "display_hal.h"
#include "bitmaps.h"
#include "font5x7.h"
#include "font8x16.h"
#include "zephyr/sys/printk.h"
 
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <stdio.h>

digee_state_t prev_s = {0};

static bool first_draw = true;

void ui_boot(const char *version) {
    oled_clear();

    int total_frames = 46;
    int str_len = strlen(version);
    int text_start_frame = 39;  // frame 23
    int text_end_frame = total_frames + 10;   // frame 56, 10 after animation ends

    for (int i = 0; i < text_end_frame; i++) {
        // draw logo frame, hold last frame once animation is done
        int frame_idx = (i < total_frames) ? i : total_frames - 1;
        oled_draw_bitmap(digeeBootup_frames[frame_idx], 6, 20,  116, 25);

        // spell out version slowly frame by frame from halfway, finishing at text_end_frame
        if (i >= text_start_frame) {
            int text_progress = i - text_start_frame;
            int text_duration = text_end_frame - text_start_frame;  // 33 frames
            int chars_to_show = (text_progress * str_len) / text_duration + 1;
            if (chars_to_show > str_len) chars_to_show = str_len;

            for (int c = 0; c < chars_to_show; c++) {
                oled_draw_char_small(version[c], c * (FONT5X7_WIDTH + 1), 0, false);
            }
        }
        display_hal_flush();
        k_msleep(33);
    }
    k_msleep(1000);
    oled_clear();
}

/* ------------------------------------------------------------------ */
/* Full redraw                                                          */
/* ------------------------------------------------------------------ */

/* Clears the buffer and redraws all UI elements from current state. */
void ui_draw_full(const digee_state_t *s) {
    if (first_draw) {
        first_draw = false;
        ui_draw_bpm(s);
        ui_draw_fraction(s);
        ui_draw_playstate(s);
        ui_draw_mathstate(s);
        ui_draw_digits(s);
        display_hal_flush();

        // Update internal UI struct
        prev_s = *s;
    } else {
        // Always draw playstate first, independent of everything else
        if (s->pause_state != prev_s.pause_state) {
            ui_draw_playstate(s);
        }
        // Draw divide state second, both icons eliminated
        if (s->divide_state != prev_s.divide_state) {
            ui_draw_mathstate(s);
        }
        // Draw bpm third, its most common thing to be updated
        if (s->bpm_main != prev_s.bpm_main) {
            ui_draw_bpm(s);
        }
        // Draw buttons
        if (s->binary != prev_s.binary) {
            ui_draw_digits(s);
        }
        // Changes the most so for now always draw it
        ui_draw_fraction(s);

        display_hal_flush();
        // Update internal UI struct
        prev_s = *s;
    }    
}

/* ------------------------------------------------------------------ */
/* Partial redraws                                                      */
/* ------------------------------------------------------------------ */

uint8_t bpm_x_offset(uint16_t bpm) {
    if (bpm >= 1000) return BPM_X_QUAD;     /* 4 digits */
    if (bpm >= 100)  return BPM_X_TRIPLE;   /* 3 digits */
    if (bpm >= 10)   return BPM_X_DOUBLE;   /* 2 digits */
    return BPM_X_SINGLE; /* 1 digit, shift one more char right */
}

uint16_t calc_ui_poly(const digee_state_t *s) {
    uint16_t poly = 0;
    uint8_t tally = s->binary;
    (tally==0) ? tally = 16 : tally;
    if (s->divide_state) {
        poly = (s->bpm_main * tally) / 16;
    } else {
        poly = (s->bpm_main * 16) / tally;
    }
    return poly;
}

void ui_draw_bpm(const digee_state_t *s) {

    char bpm1[8];
    char bpm2[8];
    uint16_t ui_bpm_poly = calc_ui_poly(s);

    snprintk(bpm1, sizeof(bpm1), "%u", s->bpm_main);
    snprintk(bpm2, sizeof(bpm2), "%u", ui_bpm_poly);

    oled_draw_rect(0, 0, BPM_WIDTH, FONT_ROBOTO_HEIGHT);
    oled_write("MSTR", 1, 0, true);
 
    // oled_draw_rect(0, 32, BPM_WIDTH, FONT_ROBOTO_HEIGHT);
    // oled_write("SYNC", 1, 32, true);

    oled_clear_rect(BPM_X_QUAD, BPM_MAIN_Y, BPM_WIDTH, FONT_ROBOTO_HEIGHT+1);
    // oled_clear_rect(BPM_X_QUAD, BPM_POLY_Y, BPM_WIDTH, FONT_ROBOTO_HEIGHT+1);
    
    uint8_t bpm_main_x = bpm_x_offset(s->bpm_main);
    // uint8_t bpm_poly_x = bpm_x_offset(ui_bpm_poly);
    
    oled_write(bpm1, bpm_main_x, BPM_MAIN_Y, false);
    // oled_write(bpm2, bpm_poly_x, BPM_POLY_Y, false);
}

void ui_draw_fraction(const digee_state_t *s) {
    char fixed_str[8];
    char custom_str[8];
    
    uint8_t ui_custom = s->binary;
    if(ui_custom==0) {ui_custom = 16;}

    snprintf(fixed_str,  sizeof(fixed_str),  "%u", DIGEE_FIXED);
    snprintf(custom_str, sizeof(custom_str), "%u", ui_custom);
 
    /* Divider line */
    oled_draw_rect(FRACTION_X, 32, 18, 2);
 
    oled_clear_rect(FRACTION_X, FRACTION_TOP_Y, FRACTION_W, FRACTION_H);
    oled_clear_rect(FRACTION_X, FRACTION_BOT_Y, FRACTION_W, FRACTION_H);
 
    const char *top = s->divide_state ? custom_str : fixed_str;
    const char *bot = s->divide_state ? fixed_str  : custom_str;
 
    /* Single digit values get nudged right to sit centred */
    uint8_t top_x = (atoi(top) < 10) ? FRACTION_X_SINGLE : FRACTION_X;
    uint8_t bot_x = (atoi(bot) < 10) ? FRACTION_X_SINGLE : FRACTION_X;
 
    oled_write(top, top_x, FRACTION_TOP_Y, false);
    oled_write(bot, bot_x, FRACTION_BOT_Y, false);
}

void ui_draw_playstate(const digee_state_t *s) {
    
    oled_clear_rect(PLAYSTATE_X, PLAYSTATE_Y, PLAYSTATE_W, PLAYSTATE_H);
    
    const uint8_t *icon = s->pause_state ? pauseIcon : playIcon;
    oled_draw_bitmap(icon, PLAYSTATE_X, PLAYSTATE_Y, PLAYSTATE_W, PLAYSTATE_H);

}

void ui_draw_mathstate(const digee_state_t *s) {
    if (s->divide_state) {
        oled_draw_bitmap(dividerIcon, MATHSTATE_X, MATHSTATE_Y, MATHSTATE_W, MATHSTATE_H);
    } else {
        oled_draw_bitmap(multiplierIcon, MATHSTATE_X, MATHSTATE_Y, MATHSTATE_W, MATHSTATE_H);
    }
}

void ui_draw_digits(const digee_state_t *s) {
    for (int i=0; i<4; i++) {
        char digit[2];
        uint8_t bit = (s->binary >> (3-i)) & 1;
        snprintk(digit, sizeof(digit), "%u", bit);
        oled_clear_rect(DIGIT_X, i*FONT_ROBOTO_HEIGHT, FONT_ROBOTO_WIDTH, FONT_ROBOTO_HEIGHT);
        oled_write(digit, DIGIT_X, i*FONT_ROBOTO_HEIGHT, false);
    }
}
