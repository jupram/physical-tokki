#include "tokki_neopixel.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "tokki_board.h"

#define TOKKI_NEOPIXEL_RMT_RESOLUTION_HZ 10000000

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
