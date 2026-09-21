#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#define TOKKI_OLED_SCROLLING_TEXT_ACTION_ID "oled.scrolling_text"
#define TOKKI_OLED_SCROLLING_TEXT_DEFAULT "Hello from Tokki!"

typedef enum {
    TOKKI_DEVICE_OLED,
    TOKKI_DEVICE_SPEAKER,
    TOKKI_DEVICE_LED,
    TOKKI_DEVICE_NEOPIXEL,
    TOKKI_DEVICE_COUNT,
} tokki_device_t;

typedef esp_err_t (*tokki_action_run_fn)(void);

typedef struct {
    const char *id;
    const char *display_name;
    tokki_device_t device;
    bool cancellable;
    tokki_action_run_fn run;
} tokki_action_descriptor_t;

size_t tokki_action_count(void);
const tokki_action_descriptor_t *tokki_action_at(size_t index);
const tokki_action_descriptor_t *tokki_action_find(const char *id);
esp_err_t tokki_action_run(const char *id);
const char *tokki_device_name(tokki_device_t device);