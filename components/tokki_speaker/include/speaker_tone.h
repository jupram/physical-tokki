#pragma once

#include <stdint.h>
#include <stddef.h>

#include "tokki_speaker.h"

#define SPEAKER_SAMPLE_RATE_HZ 16000
#define SPEAKER_MAX_VOLUME_PERCENT 20
#define SPEAKER_FADE_DURATION_MS 25

typedef struct {
    unsigned start_hz;
    unsigned end_hz;
    unsigned duration_ms;
    unsigned silence_ms;
} speaker_tone_step_t;

const speaker_tone_step_t *speaker_sound_steps(tokki_speaker_sound_t sound, size_t *count);

int16_t speaker_tone_sample(unsigned sample_index, unsigned total_frames,
                            uint32_t phase);