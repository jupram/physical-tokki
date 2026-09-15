#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define TOKKI_OLED_WIDTH 128
#define TOKKI_OLED_HEIGHT 64
#define TOKKI_OLED_FRAME_SIZE (TOKKI_OLED_WIDTH * TOKKI_OLED_HEIGHT / 8)

typedef enum {
    TOKKI_OLED_ART_DRINK_WATER,
    TOKKI_OLED_ART_WATER_DROP,
    TOKKI_OLED_ART_FIRE,
} tokki_oled_art_t;

esp_err_t tokki_oled_draw_frame(const uint8_t *framebuffer, size_t size);
esp_err_t tokki_oled_render_art(uint8_t *framebuffer, size_t size,
                                tokki_oled_art_t art, unsigned frame);