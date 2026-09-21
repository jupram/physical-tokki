#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define TOKKI_OLED_WIDTH 128
#define TOKKI_OLED_HEIGHT 64
#define TOKKI_OLED_FRAME_SIZE (TOKKI_OLED_WIDTH * TOKKI_OLED_HEIGHT / 8)
#define TOKKI_OLED_MARQUEE_MAX 50
#define TOKKI_OLED_MARQUEE_FRAME_MS 45
#define TOKKI_OLED_MARQUEE_STEP_PIXELS 2

typedef enum {
    TOKKI_OLED_ART_DRINK_WATER,
    TOKKI_OLED_ART_WATER_DROP,
    TOKKI_OLED_ART_FIRE,
    TOKKI_OLED_ART_CHECKMARK,
    TOKKI_OLED_ART_THINKING,
    TOKKI_OLED_ART_HEART,
    TOKKI_OLED_ART_EXCLAMATION,
    TOKKI_OLED_ART_NIGHT_SKY,
    TOKKI_OLED_ART_SUNRISE,
} tokki_oled_art_t;

esp_err_t tokki_oled_draw_frame(const uint8_t *framebuffer, size_t size);
esp_err_t tokki_oled_render_art(uint8_t *framebuffer, size_t size,
                                tokki_oled_art_t art, unsigned frame);
int tokki_oled_marquee_width(const char *text);
esp_err_t tokki_oled_render_marquee(uint8_t *framebuffer, size_t size,
                                    const char *text, int left);
esp_err_t tokki_oled_scroll_text(const char *text);