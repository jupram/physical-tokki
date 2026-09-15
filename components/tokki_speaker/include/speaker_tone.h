#pragma once

#include <stdint.h>

#define SPEAKER_SAMPLE_RATE_HZ 16000
#define SPEAKER_MAX_VOLUME_PERCENT 20
#define SPEAKER_FADE_DURATION_MS 25

int16_t speaker_tone_sample(unsigned sample_index, unsigned total_frames,
                            uint32_t phase);