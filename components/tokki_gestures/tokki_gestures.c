#include "tokki_gestures.h"

#include <string.h>

#include "led/led_actions.h"
#include "neopixel/neopixel_actions.h"
#include "oled/oled_actions.h"
#include "speaker/speaker_actions.h"

static const tokki_action_descriptor_t *const ACTIONS[] = {
    &TOKKI_LED_BLINK_ACTION,
    &TOKKI_NEOPIXEL_RAINBOW_ACTION,
    &TOKKI_NEOPIXEL_BLINK_RED_ACTION,
    &TOKKI_NEOPIXEL_BLINK_YELLOW_ACTION,
    &TOKKI_NEOPIXEL_BLINK_GREEN_ACTION,
    &TOKKI_NEOPIXEL_BREATHE_TEAL_ACTION,
    &TOKKI_NEOPIXEL_PULSE_BLUE_ACTION,
    &TOKKI_OLED_HAPPY_ACTION,
    &TOKKI_OLED_SAD_ACTION,
    &TOKKI_OLED_SURPRISED_ACTION,
    &TOKKI_OLED_BLINK_ACTION,
    &TOKKI_OLED_CURIOUS_ACTION,
    &TOKKI_OLED_DRINK_WATER_ACTION,
    &TOKKI_OLED_WATER_DROP_ACTION,
    &TOKKI_OLED_FIRE_ACTION,
    &TOKKI_OLED_WINK_ACTION,
    &TOKKI_OLED_CHECKMARK_ACTION,
    &TOKKI_OLED_THINKING_ACTION,
    &TOKKI_OLED_LOOK_LEFT_ACTION,
    &TOKKI_OLED_LOOK_RIGHT_ACTION,
    &TOKKI_OLED_LOOK_UP_ACTION,
    &TOKKI_OLED_LOOK_DOWN_ACTION,
    &TOKKI_OLED_SLEEPY_ACTION,
    &TOKKI_OLED_HEART_ACTION,
    &TOKKI_OLED_EXCLAMATION_ACTION,
    &TOKKI_SPEAKER_DRINK_WATER_ACTION,
    &TOKKI_SPEAKER_CHIRP_ACTION,
    &TOKKI_SPEAKER_ALERT_ACTION,
    &TOKKI_SPEAKER_CHIME_ACTION,
    &TOKKI_SPEAKER_PING_ACTION,
    &TOKKI_SPEAKER_DOG_BARK_ACTION,
};

size_t tokki_action_count(void)
{
    return sizeof(ACTIONS) / sizeof(ACTIONS[0]);
}

const tokki_action_descriptor_t *tokki_action_at(size_t index)
{
    return index < tokki_action_count() ? ACTIONS[index] : NULL;
}

const tokki_action_descriptor_t *tokki_action_find(const char *id)
{
    if (id == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < tokki_action_count(); ++index) {
        if (strcmp(ACTIONS[index]->id, id) == 0) {
            return ACTIONS[index];
        }
    }
    return NULL;
}

esp_err_t tokki_action_run(const char *id)
{
    const tokki_action_descriptor_t *action = tokki_action_find(id);
    if (action == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    if (action->run == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return action->run();
}

const char *tokki_device_name(tokki_device_t device)
{
    switch (device) {
        case TOKKI_DEVICE_OLED:
            return "oled";
        case TOKKI_DEVICE_SPEAKER:
            return "speaker";
        case TOKKI_DEVICE_LED:
            return "led";
        case TOKKI_DEVICE_NEOPIXEL:
            return "neopixel";
        default:
            return "unknown";
    }
}