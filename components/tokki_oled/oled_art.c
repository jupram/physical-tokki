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
        art < TOKKI_OLED_ART_DRINK_WATER || art > TOKKI_OLED_ART_FIRE) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(framebuffer, 0, size);
    if (art == TOKKI_OLED_ART_DRINK_WATER) {
        draw_word(framebuffer, "Drink", 15);
        draw_word(framebuffer, "water", 37);
        return ESP_OK;
    }

    static const int motion[] = {0, 1, 2, 1, 0, -1, -2, -1};
    int offset = motion[(frame / 3) % 8];
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