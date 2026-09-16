#include "led/led_actions.h"

#include "tokki_led.h"

static esp_err_t run_led_blink(void)
{
    return tokki_led_blink(3, 300, 300);
}

const tokki_action_descriptor_t TOKKI_LED_BLINK_ACTION = {
    .id = "led.blink",
    .display_name = "Blink status LED",
    .device = TOKKI_DEVICE_LED,
    .cancellable = false,
    .run = run_led_blink,
};