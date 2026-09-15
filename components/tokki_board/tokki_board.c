#include "tokki_board.h"

esp_err_t tokki_board_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << TOKKI_BOARD_PERIPHERAL_POWER_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return gpio_set_level(TOKKI_BOARD_PERIPHERAL_POWER_GPIO, 1);
}