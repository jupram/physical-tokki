#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "freertos/task.h"
#include "tokki_board.h"
#include "tokki_led.h"

static esp_err_t config_result;
static esp_err_t write_result;
static uint32_t levels[32];
static unsigned writes;
static unsigned elapsed;
static bool locked;

void test_enter_critical(portMUX_TYPE *lock)
{
    assert(lock != NULL && !locked);
    locked = true;
}

void test_exit_critical(portMUX_TYPE *lock)
{
    assert(lock != NULL && locked);
    locked = false;
}

esp_err_t gpio_config(const gpio_config_t *config)
{
    assert(config->pin_bit_mask == (1ULL << TOKKI_BOARD_STATUS_LED_GPIO));
    assert(config->mode == GPIO_MODE_OUTPUT);
    return config_result;
}

esp_err_t gpio_set_level(int gpio, uint32_t level)
{
    assert(locked && gpio == TOKKI_BOARD_STATUS_LED_GPIO && writes < 32);
    levels[writes++] = level;
    return write_result;
}

void vTaskDelay(TickType_t ticks)
{
    assert(!locked);
    elapsed += ticks;
}

int main(int argc, char **argv)
{
    assert(tokki_led_set(false) == ESP_ERR_INVALID_STATE);
    if (argc == 2 && strcmp(argv[1], "--early-failure") == 0) {
        config_result = ESP_FAIL;
        assert(tokki_led_init() == ESP_FAIL);
        assert(tokki_led_latch_failure() == ESP_ERR_INVALID_STATE);
        config_result = ESP_OK;
        assert(tokki_led_init() == ESP_OK);
        assert(writes == 1 && levels[0] == 1);
        assert(tokki_led_set(false) == ESP_OK && levels[1] == 1);
        puts("PASS: LED initialization failure retained across successful retry");
        return 0;
    }

    assert(tokki_led_init() == ESP_OK);
    assert(writes == 1 && levels[0] == 0);
    writes = 0;
    assert(tokki_led_blink(3, 200, 200) == ESP_OK);
    assert(writes == 6 && elapsed == 1200);
    for (unsigned index = 0; index < writes; ++index) {
        assert(levels[index] == (index % 2 == 0 ? 1U : 0U));
    }
    write_result = ESP_FAIL;
    assert(tokki_led_latch_failure() == ESP_FAIL);
    write_result = ESP_OK;
    writes = 0;
    assert(tokki_led_set(false) == ESP_OK);
    assert(tokki_led_blink(3, 200, 200) == ESP_OK);
    assert(tokki_led_init() == ESP_OK);
    assert(tokki_led_latch_failure() == ESP_OK);
    for (unsigned index = 0; index < writes; ++index) {
        assert(levels[index] == 1);
    }
    write_result = ESP_FAIL;
    assert(tokki_led_set(false) == ESP_FAIL);
    assert(tokki_led_blink(1, 200, 200) == ESP_FAIL);
    assert(!locked);
    puts("PASS: normal LED blinking, sticky fault priority, reinitialization and GPIO failures");
    return 0;
}
