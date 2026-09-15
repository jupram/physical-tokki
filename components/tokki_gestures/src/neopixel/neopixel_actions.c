#include "neopixel/neopixel_actions.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tokki_neopixel.h"

static esp_err_t blink_color(uint8_t red, uint8_t green, uint8_t blue)
{
    for (unsigned blink = 0; blink < 3; ++blink) {
        esp_err_t err = tokki_neopixel_set_color(red, green, blue);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        err = tokki_neopixel_set_color(0, 0, 0);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return ESP_OK;
}

static esp_err_t run_neopixel_blink_red(void)
{
    return blink_color(32, 0, 0);
}

static esp_err_t run_neopixel_blink_yellow(void)
{
    return blink_color(32, 32, 0);
}

static esp_err_t run_neopixel_blink_green(void)
{
    return blink_color(0, 32, 0);
}

static esp_err_t fade_color(bool teal, unsigned half_steps)
{
    for (unsigned step = 0; step <= half_steps * 2; ++step) {
        unsigned distance = step <= half_steps ? step : half_steps * 2 - step;
        uint8_t brightness = (uint8_t) (32 * distance / half_steps);
        esp_err_t err = tokki_neopixel_set_color(0, teal ? brightness : 0, brightness);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    return ESP_OK;
}

static esp_err_t run_neopixel_breathe_teal(void)
{
    return fade_color(true, 16);
}

static esp_err_t run_neopixel_pulse_blue(void)
{
    return fade_color(false, 8);
}

static esp_err_t run_neopixel_rainbow(void)
{
    esp_err_t err = tokki_neopixel_rainbow(1);
    if (err != ESP_OK) {
        return err;
    }
    return tokki_neopixel_set_color(0, 0, 0);
}

const tokki_action_descriptor_t TOKKI_NEOPIXEL_RAINBOW_ACTION = {
    .id = "neopixel.rainbow",
    .display_name = "Rainbow",
    .device = TOKKI_DEVICE_NEOPIXEL,
    .cancellable = false,
    .run = run_neopixel_rainbow,
};

const tokki_action_descriptor_t TOKKI_NEOPIXEL_BLINK_RED_ACTION = {
    .id = "neopixel.blink_red",
    .display_name = "Blink red",
    .device = TOKKI_DEVICE_NEOPIXEL,
    .cancellable = false,
    .run = run_neopixel_blink_red,
};

const tokki_action_descriptor_t TOKKI_NEOPIXEL_BLINK_YELLOW_ACTION = {
    .id = "neopixel.blink_yellow",
    .display_name = "Blink yellow",
    .device = TOKKI_DEVICE_NEOPIXEL,
    .cancellable = false,
    .run = run_neopixel_blink_yellow,
};

const tokki_action_descriptor_t TOKKI_NEOPIXEL_BLINK_GREEN_ACTION = {
    .id = "neopixel.blink_green",
    .display_name = "Blink green",
    .device = TOKKI_DEVICE_NEOPIXEL,
    .cancellable = false,
    .run = run_neopixel_blink_green,
};

const tokki_action_descriptor_t TOKKI_NEOPIXEL_BREATHE_TEAL_ACTION = {
    .id = "neopixel.breathe_teal", .display_name = "Breathe teal",
    .device = TOKKI_DEVICE_NEOPIXEL, .cancellable = false, .run = run_neopixel_breathe_teal,
};
const tokki_action_descriptor_t TOKKI_NEOPIXEL_PULSE_BLUE_ACTION = {
    .id = "neopixel.pulse_blue", .display_name = "Pulse blue",
    .device = TOKKI_DEVICE_NEOPIXEL, .cancellable = false, .run = run_neopixel_pulse_blue,
};