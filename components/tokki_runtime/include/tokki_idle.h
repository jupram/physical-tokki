#pragma once

#include "esp_err.h"

#define TOKKI_IDLE_FRAME_MS 60
#define TOKKI_IDLE_RETRY_MS 5000

typedef struct {
    unsigned phase;
    unsigned frame;
} tokki_idle_t;

void tokki_idle_reset(tokki_idle_t *idle);
esp_err_t tokki_idle_step(tokki_idle_t *idle);
