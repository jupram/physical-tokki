#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tokki_board.h"
#include "tokki_gestures.h"
#include "tokki_led.h"
#include "tokki_neopixel.h"

static const char *TAG = "physical_tokki";

static bool initialize_device(const char *name, esp_err_t result)
{
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "%s ready", name);
        return true;
    }

    ESP_LOGE(TAG, "%s initialization failed: %s",
             name,
             esp_err_to_name(result));
    return false;
}

static void log_action_catalog(void)
{
    ESP_LOGI(TAG, "%u actions available", (unsigned int) tokki_action_count());
    for (size_t index = 0; index < tokki_action_count(); ++index) {
        const tokki_action_descriptor_t *action = tokki_action_at(index);
        ESP_LOGI(TAG, "action=%s device=%s name=%s",
                 action->id,
                 tokki_device_name(action->device),
                 action->display_name);
    }
}

void app_main(void)
{
    bool ready = true;
    ready &= initialize_device("Board", tokki_board_init());
    ready &= initialize_device("Status LED", tokki_led_init());
    ready &= initialize_device("NeoPixel", tokki_neopixel_init());

    log_action_catalog();
    tokki_neopixel_set_color(ready ? 0 : 16, ready ? 16 : 0, 0);

    while (true) {
        tokki_led_blink(1, 100, 1900);
    }
}