#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t tokki_neopixel_init(void);
esp_err_t tokki_neopixel_set_color(uint8_t red,
                                   uint8_t green,
                                   uint8_t blue);
esp_err_t tokki_neopixel_rainbow(uint32_t cycles);
/* 128 steps per cycle; frame_ms must be 1..1000. Retains the last color. */
esp_err_t tokki_neopixel_rainbow_timed(uint32_t cycles, uint32_t frame_ms);