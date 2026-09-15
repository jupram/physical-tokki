#include "speaker_tone.h"

_Static_assert(SPEAKER_MAX_VOLUME_PERCENT <= 20,
               "Speaker volume must never exceed 20 percent");

static const int16_t SINE_TABLE[32] = {
    0, 6393, 12539, 18204, 23170, 27245, 30273, 32137,
    32767, 32137, 30273, 27245, 23170, 18204, 12539, 6393,
    0, -6393, -12539, -18204, -23170, -27245, -30273, -32137,
    -32767, -32137, -30273, -27245, -23170, -18204, -12539, -6393,
};

const speaker_tone_step_t *speaker_sound_steps(tokki_speaker_sound_t sound, size_t *count)
{
    static const speaker_tone_step_t chirp[] = {{1800, 3000, 120, 80}, {2200, 3600, 120, 0}};
    static const speaker_tone_step_t alert[] = {{660, 660, 180, 0}};
    static const speaker_tone_step_t chime[] = {{784, 784, 160, 60}, {1047, 1047, 240, 0}};
    static const speaker_tone_step_t ping[] = {{1320, 1320, 100, 0}};
    if (count == NULL) {
        return NULL;
    }
    *count = 0;
    switch (sound) {
    case TOKKI_SPEAKER_CHIRP:
        *count = sizeof(chirp) / sizeof(chirp[0]);
        return chirp;
    case TOKKI_SPEAKER_ALERT:
        *count = sizeof(alert) / sizeof(alert[0]);
        return alert;
    case TOKKI_SPEAKER_CHIME:
        *count = sizeof(chime) / sizeof(chime[0]);
        return chime;
    case TOKKI_SPEAKER_PING:
        *count = sizeof(ping) / sizeof(ping[0]);
        return ping;
    default:
        return NULL;
    }
}

int16_t speaker_tone_sample(unsigned sample_index, unsigned total_frames,
                            uint32_t phase)
{
    if (sample_index >= total_frames) {
        return 0;
    }
    const unsigned fade_frames = SPEAKER_SAMPLE_RATE_HZ * SPEAKER_FADE_DURATION_MS / 1000;
    unsigned volume = SPEAKER_MAX_VOLUME_PERCENT;
    if (sample_index < fade_frames) {
        volume = SPEAKER_MAX_VOLUME_PERCENT * sample_index / fade_frames;
    }
    unsigned remaining = total_frames - sample_index - 1;
    if (remaining < fade_frames) {
        unsigned fade_out = SPEAKER_MAX_VOLUME_PERCENT * remaining / fade_frames;
        if (fade_out < volume) {
            volume = fade_out;
        }
    }
    return (int16_t) ((int32_t) SINE_TABLE[phase >> 27] * (int32_t) volume / 100);
}