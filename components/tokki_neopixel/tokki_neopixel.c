#include "tokki_neopixel.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tokki_board.h"

#define TOKKI_NEOPIXEL_RMT_RESOLUTION_HZ 10000000
#define TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS 32
#define TOKKI_NEOPIXEL_RAINBOW_STEPS 128
#define TOKKI_NEOPIXEL_RAINBOW_FRAME_MS 20

static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;

esp_err_t tokki_neopixel_init(void)
{
    rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = TOKKI_BOARD_NEOPIXEL_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = TOKKI_NEOPIXEL_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };

    esp_err_t err = rmt_new_tx_channel(&channel_config, &s_channel);
    if (err != ESP_OK) {
        return err;
    }

    rmt_copy_encoder_config_t encoder_config = {};
    err = rmt_new_copy_encoder(&encoder_config, &s_encoder);
    if (err != ESP_OK) {
        rmt_del_channel(s_channel);
        s_channel = NULL;
        return err;
    }

    err = rmt_enable(s_channel);
    if (err != ESP_OK) {
        rmt_del_encoder(s_encoder);
        rmt_del_channel(s_channel);
        s_encoder = NULL;
        s_channel = NULL;
    }
    return err;
}

esp_err_t tokki_neopixel_set_color(uint8_t red,
                                   uint8_t green,
                                   uint8_t blue)
{
    if (s_channel == NULL || s_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t grb[] = {green, red, blue};
    rmt_symbol_word_t symbols[25] = {};
    size_t symbol_index = 0;

    for (size_t byte_index = 0; byte_index < sizeof(grb); ++byte_index) {
        for (int bit_index = 7; bit_index >= 0; --bit_index) {
            bool one = (grb[byte_index] & (1U << bit_index)) != 0;
            symbols[symbol_index++] = (rmt_symbol_word_t) {
                .level0 = 1,
                .duration0 = one ? 9 : 3,
                .level1 = 0,
                .duration1 = one ? 3 : 9,
            };
        }
    }

    symbols[symbol_index] = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = 250,
        .level1 = 0,
        .duration1 = 250,
    };

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(
        s_channel,
        s_encoder,
        symbols,
        sizeof(symbols),
        &transmit_config
    );
    if (err != ESP_OK) {
        return err;
    }

    return rmt_tx_wait_all_done(s_channel, pdMS_TO_TICKS(100));
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

    *red = raw_red * TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS / 255;
    *green = raw_green * TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS / 255;
    *blue = raw_blue * TOKKI_NEOPIXEL_RAINBOW_BRIGHTNESS / 255;
}

esp_err_t tokki_neopixel_rainbow(uint32_t cycles)
{
    for (uint32_t cycle = 0; cycle < cycles; ++cycle) {
        for (int step = 0; step < TOKKI_NEOPIXEL_RAINBOW_STEPS; ++step) {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t position = step * 256 / TOKKI_NEOPIXEL_RAINBOW_STEPS;
            rainbow_color(position, &red, &green, &blue);

            esp_err_t err = tokki_neopixel_set_color(red, green, blue);
            if (err != ESP_OK) {
                return err;
            }
            vTaskDelay(pdMS_TO_TICKS(TOKKI_NEOPIXEL_RAINBOW_FRAME_MS));
        }
    }
    return ESP_OK;
}