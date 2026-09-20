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
    static const speaker_tone_step_t chirp[] = {{1800, 3000, 120, 80, 100}, {2200, 3600, 120, 0, 100}};
    static const speaker_tone_step_t alert[] = {{660, 660, 180, 0, 100}};
    static const speaker_tone_step_t chime[] = {{784, 784, 160, 60, 100}, {1047, 1047, 240, 0, 100}};
    static const speaker_tone_step_t ping[] = {{1320, 1320, 100, 0, 100}};
    static const speaker_tone_step_t bubble[] = {{1200, 480, 90, 0, 100}};
    static const speaker_tone_step_t whistle[] = {{900, 2100, 280, 0, 100}};
    static const speaker_tone_step_t tone_low[] = {
        {SPEAKER_TONE_LOW_HZ, SPEAKER_TONE_LOW_HZ, SPEAKER_REFERENCE_TONE_DURATION_MS, 0, 100},
    };
    static const speaker_tone_step_t tone_mid[] = {
        {SPEAKER_TONE_MID_HZ, SPEAKER_TONE_MID_HZ, SPEAKER_REFERENCE_TONE_DURATION_MS, 0, 100},
    };
    static const speaker_tone_step_t question[] = {{700, 700, 90, 50, 100}, {950, 1250, 180, 0, 100}};
    static const speaker_tone_step_t tone_high[] = {
        {SPEAKER_TONE_HIGH_HZ, SPEAKER_TONE_HIGH_HZ, SPEAKER_REFERENCE_TONE_DURATION_MS, 0, 100},
    };
    static const speaker_tone_step_t sparkle[] = {{1047, 1047, 70, 30, 100}, {1319, 1319, 70, 30, 100}, {1568, 1568, 130, 0, 100}};
    static const speaker_tone_step_t trill[] = {{1400, 1700, 70, 25, 100}, {1400, 1700, 70, 25, 100}, {1400, 1700, 70, 0, 100}};
    static const speaker_tone_step_t knock[] = {{500, 200, 65, 100, 100}, {500, 200, 65, 0, 100}};
    static const speaker_tone_step_t sonar[] = {{880, 880, 120, 110, 100}, {880, 880, 120, 0, 40}};
    static const speaker_tone_step_t tone_rise[] = {
        {SPEAKER_TONE_LOW_HZ, SPEAKER_TONE_LOW_HZ, 200, 60, 100},
        {SPEAKER_TONE_MID_HZ, SPEAKER_TONE_MID_HZ, 200, 60, 100},
        {SPEAKER_TONE_HIGH_HZ, SPEAKER_TONE_HIGH_HZ, 200, 0, 100},
    };
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
    case TOKKI_SPEAKER_BUBBLE:
        *count = sizeof(bubble) / sizeof(bubble[0]);
        return bubble;
    case TOKKI_SPEAKER_WHISTLE:
        *count = sizeof(whistle) / sizeof(whistle[0]);
        return whistle;
    case TOKKI_SPEAKER_TONE_LOW:
        *count = sizeof(tone_low) / sizeof(tone_low[0]);
        return tone_low;
    case TOKKI_SPEAKER_TONE_MID:
        *count = sizeof(tone_mid) / sizeof(tone_mid[0]);
        return tone_mid;
    case TOKKI_SPEAKER_QUESTION:
        *count = sizeof(question) / sizeof(question[0]);
        return question;
    case TOKKI_SPEAKER_TONE_HIGH:
        *count = sizeof(tone_high) / sizeof(tone_high[0]);
        return tone_high;
    case TOKKI_SPEAKER_SPARKLE:
        *count = sizeof(sparkle) / sizeof(sparkle[0]);
        return sparkle;
    case TOKKI_SPEAKER_TRILL:
        *count = sizeof(trill) / sizeof(trill[0]);
        return trill;
    case TOKKI_SPEAKER_KNOCK:
        *count = sizeof(knock) / sizeof(knock[0]);
        return knock;
    case TOKKI_SPEAKER_SONAR:
        *count = sizeof(sonar) / sizeof(sonar[0]);
        return sonar;
    case TOKKI_SPEAKER_TONE_RISE:
        *count = sizeof(tone_rise) / sizeof(tone_rise[0]);
        return tone_rise;
    default:
        return NULL;
    }
}

static int16_t envelope_sample(int32_t raw, unsigned sample_index, unsigned total_frames)
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
    return (int16_t) (raw * (int32_t) volume / 100);
}

int16_t speaker_tone_sample(unsigned sample_index, unsigned total_frames,
                            uint32_t phase)
{
    return envelope_sample(SINE_TABLE[phase >> 27], sample_index, total_frames);
}

int16_t speaker_tone_scaled_sample(unsigned sample_index, unsigned total_frames,
                                   uint32_t phase, unsigned gain_percent)
{
    unsigned gain = gain_percent > 100 ? 100 : gain_percent;
    int16_t sample = speaker_tone_sample(sample_index, total_frames, phase);
    return (int16_t) ((int32_t) sample * (int32_t) gain / 100);
}

unsigned speaker_step_frequency(const speaker_tone_step_t *step,
                                 unsigned sample_index, unsigned total_frames)
{
    if (total_frames == 0 || sample_index >= total_frames) {
        return step->end_hz;
    }
    int64_t delta = (int64_t) step->end_hz - step->start_hz;
    return (unsigned) ((int64_t) step->start_hz + delta * sample_index / total_frames);
}
