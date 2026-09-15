#include "neopixel/neopixel_actions.h"

#include "tokki_neopixel.h"

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