#pragma once

#include "esp_err.h"

typedef enum {
    TOKKI_SPEAKER_DRINK_WATER,
    TOKKI_SPEAKER_CHIRP,
    TOKKI_SPEAKER_ALERT,
} tokki_speaker_sound_t;

esp_err_t tokki_speaker_play(tokki_speaker_sound_t sound);