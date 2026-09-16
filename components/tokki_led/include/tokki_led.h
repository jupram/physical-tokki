#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t tokki_led_init(void);
esp_err_t tokki_led_set(bool enabled);
/* Keeps the red LED on until reset; subsequent set/blink calls cannot clear it. */
esp_err_t tokki_led_latch_failure(void);
esp_err_t tokki_led_blink(uint32_t count,
                          uint32_t on_duration_ms,
                          uint32_t off_duration_ms);