#include "tokki_led.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tokki_board.h"

static bool s_initialized;
static bool s_failure_latched;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t tokki_led_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << TOKKI_BOARD_STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }

    portENTER_CRITICAL(&s_lock);
    err = gpio_set_level(TOKKI_BOARD_STATUS_LED_GPIO, s_failure_latched ? 1 : 0);
    if (err == ESP_OK) {
        s_initialized = true;
    }
    portEXIT_CRITICAL(&s_lock);
    return err;
}

esp_err_t tokki_led_set(bool enabled)
{
    portENTER_CRITICAL(&s_lock);
    esp_err_t err = s_initialized
        ? gpio_set_level(TOKKI_BOARD_STATUS_LED_GPIO, (enabled || s_failure_latched) ? 1 : 0)
        : ESP_ERR_INVALID_STATE;
    portEXIT_CRITICAL(&s_lock);
    return err;
}

esp_err_t tokki_led_latch_failure(void)
{
    portENTER_CRITICAL(&s_lock);
    s_failure_latched = true;
    esp_err_t err = s_initialized
        ? gpio_set_level(TOKKI_BOARD_STATUS_LED_GPIO, 1)
        : ESP_ERR_INVALID_STATE;
    portEXIT_CRITICAL(&s_lock);
    return err;
}

esp_err_t tokki_led_blink(uint32_t count,
                          uint32_t on_duration_ms,
                          uint32_t off_duration_ms)
{
    for (uint32_t index = 0; index < count; ++index) {
        esp_err_t err = tokki_led_set(true);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(on_duration_ms));

        err = tokki_led_set(false);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(off_duration_ms));
    }

    return ESP_OK;
}