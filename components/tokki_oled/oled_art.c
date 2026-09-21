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

static const glyph_t MARQUEE_FONT[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
    {'#', {0x14, 0x7F, 0x14, 0x7F, 0x14}},
    {'&', {0x36, 0x49, 0x55, 0x22, 0x50}},
    {'\'', {0x00, 0x05, 0x03, 0x00, 0x00}},
    {'(', {0x00, 0x1C, 0x22, 0x41, 0x00}},
    {')', {0x00, 0x41, 0x22, 0x1C, 0x00}},
    {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}},
    {',', {0x00, 0x50, 0x30, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'/', {0x20, 0x10, 0x08, 0x04, 0x02}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {';', {0x00, 0x56, 0x36, 0x00, 0x00}},
    {'?', {0x02, 0x01, 0x51, 0x09, 0x06}},
    {'@', {0x32, 0x49, 0x79, 0x41, 0x3E}},
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
    {'_', {0x40, 0x40, 0x40, 0x40, 0x40}},
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

static void draw_line(uint8_t *framebuffer, int start_x, int start_y,
                      int end_x, int end_y, int radius)
{
    int delta_x = end_x - start_x;
    int delta_y = end_y - start_y;
    int steps = (delta_x < 0 ? -delta_x : delta_x) + (delta_y < 0 ? -delta_y : delta_y);
    for (int step = 0; step <= steps; ++step) {
        int column = start_x + (steps == 0 ? 0 : delta_x * step / steps);
        int row = start_y + (steps == 0 ? 0 : delta_y * step / steps);
        draw_disc(framebuffer, column, row, radius);
    }
}

static void draw_stroke(uint8_t *framebuffer, int start_x, int start_y,
                        int end_x, int end_y)
{
    draw_line(framebuffer, start_x, start_y, end_x, end_y, 2);
}

static void draw_thin_line(uint8_t *framebuffer, int start_x, int start_y,
                           int end_x, int end_y)
{
    draw_line(framebuffer, start_x, start_y, end_x, end_y, 0);
}

static void draw_night_sky(uint8_t *framebuffer, unsigned frame)
{
    static const uint8_t stars[][3] = {
        {10, 12, 0}, {29, 8, 5}, {52, 16, 9}, {75, 8, 2},
        {17, 33, 7}, {38, 26, 12}, {67, 35, 4}, {86, 42, 10},
        {113, 38, 14}, {46, 43, 1},
    };
    static const uint8_t twinkle[] = {0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0};
    unsigned phase = frame % 48;
    for (size_t star = 0; star < sizeof(stars) / sizeof(stars[0]); ++star) {
        int radius = twinkle[(phase / 3 + stars[star][2]) % 16];
        int x = stars[star][0];
        int y = stars[star][1];
        draw_thin_line(framebuffer, x - radius, y, x + radius, y);
        draw_thin_line(framebuffer, x, y - radius, x, y + radius);
    }
    for (int y = -11; y <= 11; ++y) {
        for (int x = -11; x <= 11; ++x) {
            if (x * x + y * y <= 121 && (x - 5) * (x - 5) + (y + 3) * (y + 3) > 100) {
                set_pixel(framebuffer, 102 + x, 17 + y);
            }
        }
    }
    if (phase >= 20 && phase < 32) {
        int travel = (int) phase - 20;
        int x = 26 + travel * 4;
        int y = 19 + travel;
        draw_thin_line(framebuffer, x - 6, y - 2, x, y);
        draw_disc(framebuffer, x, y, 1);
    }
    for (int x = 0; x < TOKKI_OLED_WIDTH; ++x) {
        int distance = x % 48 - 24;
        if (distance < 0) distance = -distance;
        set_pixel(framebuffer, x, 55 + distance / 4);
    }
}

static void draw_sunrise(uint8_t *framebuffer, unsigned frame)
{
    // Saturate before integer easing so large frame indices cannot overflow.
    int progress = frame < 36 ? (int) frame : 36;
    int rise = 30 * progress * progress * (108 - 2 * progress) / (36 * 36 * 36);
    int center_y = 60 - rise;
    for (int y = -12; y <= 12; ++y) {
        for (int x = -12; x <= 12; ++x) {
            if (x * x + y * y <= 144 && center_y + y < 48) {
                set_pixel(framebuffer, 64 + x, center_y + y);
            }
        }
    }
    static const int rays[][2] = {
        {-16, 0}, {-14, -8}, {-8, -14}, {0, -16}, {8, -14}, {14, -8}, {16, 0},
    };
    if (frame >= 18) {
        int extension = frame < 42 ? (int) (frame - 18) / 4 + 1 : 7;
        for (size_t ray = 0; ray < sizeof(rays) / sizeof(rays[0]); ++ray) {
            int x = rays[ray][0];
            int y = rays[ray][1];
            if (center_y + y < 48) {
                draw_thin_line(framebuffer, 64 + x, center_y + y,
                               64 + x * (16 + extension) / 16,
                               center_y + y * (16 + extension) / 16);
            }
        }
    }
    draw_thin_line(framebuffer, 8, 48, 119, 48);
    draw_thin_line(framebuffer, 8, 49, 119, 49);
    draw_thin_line(framebuffer, 45, 54, 83, 54);
    draw_thin_line(framebuffer, 53, 59, 75, 59);
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
        art < TOKKI_OLED_ART_DRINK_WATER || art > TOKKI_OLED_ART_SUNRISE) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(framebuffer, 0, size);
    if (art == TOKKI_OLED_ART_NIGHT_SKY) {
        draw_night_sky(framebuffer, frame);
        return ESP_OK;
    }
    if (art == TOKKI_OLED_ART_SUNRISE) {
        draw_sunrise(framebuffer, frame);
        return ESP_OK;
    }
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

static const uint8_t *marquee_glyph(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = (char) (character - 'a' + 'A');
    }
    for (size_t index = 0; index < sizeof(MARQUEE_FONT) / sizeof(MARQUEE_FONT[0]); ++index) {
        if (MARQUEE_FONT[index].character == character) {
            return MARQUEE_FONT[index].columns;
        }
    }
    return marquee_glyph('?');
}

int tokki_oled_marquee_width(const char *text)
{
    if (text == NULL || text[0] == '\0' || strlen(text) > TOKKI_OLED_MARQUEE_MAX) {
        return 0;
    }
    return (int) strlen(text) * 12 - 2;
}

esp_err_t tokki_oled_render_marquee(uint8_t *framebuffer, size_t size,
                                    const char *text, int left)
{
    if (framebuffer == NULL || size != TOKKI_OLED_FRAME_SIZE ||
        tokki_oled_marquee_width(text) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(framebuffer, 0, size);
    for (size_t character = 0; text[character] != '\0'; ++character) {
        const uint8_t *glyph = marquee_glyph(text[character]);
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if ((glyph[column] & (1U << row)) == 0) {
                    continue;
                }
                for (int vertical = 0; vertical < 2; ++vertical) {
                    for (int horizontal = 0; horizontal < 2; ++horizontal) {
                        set_pixel(framebuffer,
                                  left + (int) character * 12 + column * 2 + horizontal,
                                  25 + row * 2 + vertical);
                    }
                }
            }
        }
    }
    return ESP_OK;
}