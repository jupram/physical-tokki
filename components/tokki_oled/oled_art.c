#include "tokki_oled.h"

#include <stdbool.h>
#include <string.h>

typedef struct {
    char character;
    uint8_t columns[5];
} glyph_t;

static const glyph_t FONT[] = {
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'e', {0x38, 0x54, 0x54, 0x54, 0x18}},
    {'i', {0x00, 0x44, 0x7D, 0x40, 0x00}},
    {'k', {0x7F, 0x10, 0x28, 0x44, 0x00}},
    {'n', {0x7C, 0x08, 0x04, 0x04, 0x78}},
    {'r', {0x7C, 0x08, 0x04, 0x04, 0x08}},
    {'t', {0x04, 0x3F, 0x44, 0x40, 0x20}},
    {'w', {0x3C, 0x40, 0x30, 0x40, 0x3C}},
};

static void set_pixel(uint8_t *framebuffer, int column, int row)
{
    if (column >= 0 && column < TOKKI_OLED_WIDTH &&
        row >= 0 && row < TOKKI_OLED_HEIGHT) {
        framebuffer[TOKKI_OLED_WIDTH * (row / 8) + column] |= 1U << (row % 8);
    }
}

static void draw_disc(uint8_t *framebuffer, int center_x, int center_y, int radius)
{
    for (int row = -radius; row <= radius; ++row) {
        for (int column = -radius; column <= radius; ++column) {
            if (row * row + column * column <= radius * radius) {
                set_pixel(framebuffer, center_x + column, center_y + row);
            }
        }
    }
}

static void draw_stroke(uint8_t *framebuffer, int start_x, int start_y,
                        int end_x, int end_y)
{
    int delta_x = end_x - start_x;
    int delta_y = end_y - start_y;
    int steps = (delta_x < 0 ? -delta_x : delta_x) + (delta_y < 0 ? -delta_y : delta_y);
    for (int step = 0; step <= steps; ++step) {
        int column = start_x + (steps == 0 ? 0 : delta_x * step / steps);
        int row = start_y + (steps == 0 ? 0 : delta_y * step / steps);
        draw_disc(framebuffer, column, row, 2);
    }
}

static void draw_word(uint8_t *framebuffer, const char *word, int top)
{
    int left = (TOKKI_OLED_WIDTH - ((int) strlen(word) * 12 - 2)) / 2;
    for (size_t character = 0; word[character] != '\0'; ++character) {
        for (size_t glyph = 0; glyph < sizeof(FONT) / sizeof(FONT[0]); ++glyph) {
            if (FONT[glyph].character != word[character]) {
                continue;
            }
            for (int column = 0; column < 5; ++column) {
                for (int row = 0; row < 7; ++row) {
                    if ((FONT[glyph].columns[column] & (1U << row)) != 0) {
                        for (int vertical = 0; vertical < 2; ++vertical) {
                            for (int horizontal = 0; horizontal < 2; ++horizontal) {
                                set_pixel(framebuffer, left + (int) character * 12 + column * 2 + horizontal,
                                          top + row * 2 + vertical);
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}

esp_err_t tokki_oled_render_art(uint8_t *framebuffer, size_t size,
                                tokki_oled_art_t art, unsigned frame)
{
    if (framebuffer == NULL || size != TOKKI_OLED_FRAME_SIZE ||
        art < TOKKI_OLED_ART_DRINK_WATER || art > TOKKI_OLED_ART_EXCLAMATION) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(framebuffer, 0, size);
    if (art == TOKKI_OLED_ART_DRINK_WATER) {
        draw_word(framebuffer, "Drink", 15);
        draw_word(framebuffer, "water", 37);
        return ESP_OK;
    }

    if (art == TOKKI_OLED_ART_CHECKMARK) {
        unsigned progress = frame > 8 ? 8 : frame;
        int first = progress > 3 ? 3 : (int) progress;
        draw_stroke(framebuffer, 40, 33, 40 + first * 5, 33 + first * 4);
        if (progress > 3) {
            int second = (int) progress - 3;
            draw_stroke(framebuffer, 55, 45, 55 + second * 6, 45 - second * 6);
        }
        return ESP_OK;
    }
    if (art == TOKKI_OLED_ART_THINKING) {
        unsigned active_dot = frame / 6;
        if (active_dot > 2) {
            active_dot = 2;
        }
        for (unsigned dot = 0; dot < 3; ++dot) {
            draw_disc(framebuffer, 42 + (int) dot * 22, 32,
                       dot == active_dot ? 6 : 3);
        }
        return ESP_OK;
    }

    static const int motion[] = {0, 1, 2, 1, 0, -1, -2, -1};
    int offset = motion[(frame / 3) % 8];
    if (art == TOKKI_OLED_ART_HEART) {
        draw_disc(framebuffer, 55, 26, 11 + offset);
        draw_disc(framebuffer, 73, 26, 11 + offset);
        for (int row = 26; row <= 50 + offset; ++row) {
            int half_width = (50 + offset - row) * (20 + offset) / (24 + offset);
            for (int column = 64 - half_width; column <= 64 + half_width; ++column) {
                set_pixel(framebuffer, column, row);
            }
        }
        return ESP_OK;
    }
    if (art == TOKKI_OLED_ART_EXCLAMATION) {
        int growth = frame < 5 ? (int) frame : 4;
        draw_stroke(framebuffer, 64, 34 - growth * 5, 64, 36);
        draw_disc(framebuffer, 64, 47, 3);
        return ESP_OK;
    }
    for (int row = 0; row < TOKKI_OLED_HEIGHT; ++row) {
        for (int column = 0; column < TOKKI_OLED_WIDTH; ++column) {
            int horizontal = column - 64;
            int vertical = row - 39 - offset;
            int tip_distance = row - 9 - offset;
            bool inside = false;
            if (art == TOKKI_OLED_ART_WATER_DROP) {
                inside = (vertical >= 0 && horizontal * horizontal + vertical * vertical <= 225) ||
                         (tip_distance >= 0 && tip_distance <= 30 &&
                          horizontal * horizontal * 4 <= tip_distance * tip_distance);
                if (horizontal >= -8 && horizontal <= -5 && vertical >= -2 && vertical <= 6) {
                    inside = false;
                }
            } else {
                int lean = offset * (54 - row) / 8;
                int centered = horizontal - lean;
                int lower = row - 41;
                inside = horizontal * horizontal + lower * lower <= 256;
                if (row >= 7 && row < 42 && centered * centered * 3 <= (row - 7) * (row - 7)) {
                    inside = true;
                }
                if (row >= 22 && row < 43 && (horizontal + 12) * (horizontal + 12) <= (row - 22) * 3) {
                    inside = true;
                }
                if (row > 34 && row < 56 && horizontal * horizontal * 5 < (row - 34) * (row - 34)) {
                    inside = false;
                }
            }
            if (inside) {
                set_pixel(framebuffer, column, row);
            }
        }
    }
    return ESP_OK;
}