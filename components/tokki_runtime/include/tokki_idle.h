#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define TOKKI_IDLE_FRAME_MS 60
#define TOKKI_IDLE_RETRY_MS 5000
#define TOKKI_IDLE_EYE_VARIANTS 8

typedef struct {
    uint32_t random_state;
    unsigned order[TOKKI_IDLE_EYE_VARIANTS];
    unsigned next_phase;
    unsigned phase;
    unsigned frame;
    unsigned frames;
    bool resting;
} tokki_idle_t;

typedef struct {
    uint32_t random_state;
    unsigned frame;
    unsigned delay_ms;
    bool resting;
} tokki_idle_neopixel_t;

void tokki_idle_reset(tokki_idle_t *idle, uint32_t seed);
esp_err_t tokki_idle_step(tokki_idle_t *idle);
void tokki_idle_neopixel_reset(tokki_idle_neopixel_t *idle, uint32_t seed);
esp_err_t tokki_idle_neopixel_step(tokki_idle_neopixel_t *idle);
