#include "tokki_neopixel.h"

#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS 32
#define TOKKI_NEOPIXEL_RAINBOW_STEPS 128
#define TOKKI_NEOPIXEL_RAINBOW_FRAME_MS 20

esp_err_t tokki_neopixel_fade_step(bool teal, unsigned step, unsigned half_steps)
{
    if (half_steps == 0 || half_steps > UINT_MAX / 2 || step > half_steps * 2) {
        return ESP_ERR_INVALID_ARG;
    }
    unsigned distance = step <= half_steps ? step : half_steps * 2 - step;
    uint8_t brightness = (uint8_t) ((uint64_t) 32 * distance / half_steps);
    return tokki_neopixel_set_color(0, teal ? brightness : 0, brightness);
}

static void rainbow_color(uint8_t position,
                          uint8_t *red,
                          uint8_t *green,
                          uint8_t *blue)
{
    uint16_t raw_red;
    uint16_t raw_green;
    uint16_t raw_blue;

    if (position < 85) {
        raw_red = 255 - position * 3;
        raw_green = position * 3;
        raw_blue = 0;
    } else if (position < 170) {
        position -= 85;
        raw_red = 0;
        raw_green = 255 - position * 3;
        raw_blue = position * 3;
    } else {
        position -= 170;
        raw_red = position * 3;
        raw_green = 0;
        raw_blue = 255 - position * 3;
    }

    *red = (uint8_t) (raw_red * TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS / 255);
    *green = (uint8_t) (raw_green * TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS / 255);
    *blue = (uint8_t) (raw_blue * TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS / 255);
}

esp_err_t tokki_neopixel_rainbow_timed(uint32_t cycles, uint32_t frame_ms)
{
    if (frame_ms == 0 || frame_ms > 1000) {
        return ESP_ERR_INVALID_ARG;
    }
    for (uint32_t cycle = 0; cycle < cycles; ++cycle) {
        for (int step = 0; step < TOKKI_NEOPIXEL_RAINBOW_STEPS; ++step) {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t position = (uint8_t) (step * 256 / TOKKI_NEOPIXEL_RAINBOW_STEPS);
            rainbow_color(position, &red, &green, &blue);

            esp_err_t err = tokki_neopixel_set_color(red, green, blue);
            if (err != ESP_OK) {
                return err;
            }
            vTaskDelay(pdMS_TO_TICKS(frame_ms));
        }
    }
    return ESP_OK;
}

esp_err_t tokki_neopixel_rainbow(uint32_t cycles)
{
    return tokki_neopixel_rainbow_timed(cycles, TOKKI_NEOPIXEL_RAINBOW_FRAME_MS);
}
