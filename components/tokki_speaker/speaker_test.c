#include "speaker_test.h"
#include "speaker_tone.h"
#include "tokki_speaker.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "tokki_board.h"

#define SPEAKER_TONE_DURATION_MS 300
#define SPEAKER_SILENCE_DURATION_MS 250
#define SPEAKER_FRAMES_PER_BUFFER 128

extern const uint8_t hello_ram_wav_start[]
    asm("_binary_hello_ram_wav_start");
extern const uint8_t hello_ram_wav_end[]
    asm("_binary_hello_ram_wav_end");

extern const uint8_t drink_water_wav_start[]
    asm("_binary_drink_water_wav_start");
extern const uint8_t drink_water_wav_end[]
    asm("_binary_drink_water_wav_end");

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t) data[0] | ((uint16_t) data[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t) data[0] |
           ((uint32_t) data[1] << 8) |
           ((uint32_t) data[2] << 16) |
           ((uint32_t) data[3] << 24);
}

static esp_err_t write_frames(i2s_chan_handle_t channel,
                              const int16_t *frames,
                              size_t frame_count)
{
    size_t expected_bytes = frame_count * 2 * sizeof(int16_t);
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(
        channel,
        frames,
        expected_bytes,
        &bytes_written,
        1000
    );
    if (err != ESP_OK) {
        return err;
    }
    return bytes_written == expected_bytes ? ESP_OK : ESP_FAIL;
}

static esp_err_t write_silence(i2s_chan_handle_t channel,
                               unsigned int duration_ms)
{
    int16_t frames[SPEAKER_FRAMES_PER_BUFFER * 2] = {};
    unsigned int remaining = SPEAKER_SAMPLE_RATE_HZ * duration_ms / 1000;

    while (remaining > 0) {
        size_t frame_count = remaining < SPEAKER_FRAMES_PER_BUFFER
            ? remaining
            : SPEAKER_FRAMES_PER_BUFFER;
        esp_err_t err = write_frames(channel, frames, frame_count);
        if (err != ESP_OK) {
            return err;
        }
        remaining -= frame_count;
    }
    return ESP_OK;
}

static esp_err_t write_tone(i2s_chan_handle_t channel,
                            unsigned int start_hz,
                            unsigned int end_hz,
                            unsigned int duration_ms)
{
    int16_t frames[SPEAKER_FRAMES_PER_BUFFER * 2];
    const unsigned int total_frames =
        SPEAKER_SAMPLE_RATE_HZ * duration_ms / 1000;
    uint32_t phase = 0;
    unsigned int generated = 0;

    while (generated < total_frames) {
        size_t frame_count = total_frames - generated;
        if (frame_count > SPEAKER_FRAMES_PER_BUFFER) {
            frame_count = SPEAKER_FRAMES_PER_BUFFER;
        }

        for (size_t index = 0; index < frame_count; ++index) {
            unsigned int sample_index = generated + index;
            unsigned int frequency_hz = start_hz +
                (end_hz - start_hz) * sample_index / total_frames;
            uint32_t phase_increment = (uint32_t) (
                ((uint64_t) frequency_hz << 32) / SPEAKER_SAMPLE_RATE_HZ
            );
            int16_t sample = speaker_tone_sample(sample_index, total_frames, phase);
            frames[index * 2] = sample;
            frames[index * 2 + 1] = sample;
            phase += phase_increment;
        }

        esp_err_t err = write_frames(channel, frames, frame_count);
        if (err != ESP_OK) {
            return err;
        }
        generated += frame_count;
    }
    return ESP_OK;
}

static esp_err_t find_wav_samples(const uint8_t *wav,
                                  size_t wav_size,
                                  const uint8_t **samples,
                                  size_t *sample_count)
{
    if (wav == NULL || samples == NULL || sample_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (wav_size < 12 ||
        memcmp(wav, "RIFF", 4) != 0 ||
        memcmp(wav + 8, "WAVE", 4) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    bool valid_format = false;
    size_t offset = 12;
    while (offset + 8 <= wav_size) {
        const uint8_t *chunk = wav + offset;
        uint32_t chunk_size = read_u32_le(chunk + 4);
        size_t data_offset = offset + 8;
        if (data_offset + chunk_size > wav_size) {
            return ESP_ERR_INVALID_SIZE;
        }

        if (memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
            const uint8_t *format = wav + data_offset;
            valid_format =
                read_u16_le(format) == 1 &&
                read_u16_le(format + 2) == 1 &&
                read_u32_le(format + 4) == SPEAKER_SAMPLE_RATE_HZ &&
                read_u16_le(format + 14) == 16;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!valid_format || chunk_size % sizeof(int16_t) != 0) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            *samples = wav + data_offset;
            *sample_count = chunk_size / sizeof(int16_t);
            return ESP_OK;
        }

        offset = data_offset + chunk_size + (chunk_size & 1U);
    }

    return ESP_ERR_NOT_FOUND;
}

static esp_err_t write_speech(i2s_chan_handle_t channel,
                              const uint8_t *wav, size_t wav_size)
{
    const uint8_t *source = NULL;
    size_t sample_count = 0;
    esp_err_t err = find_wav_samples(wav, wav_size, &source, &sample_count);
    if (err != ESP_OK) {
        return err;
    }

    int16_t frames[SPEAKER_FRAMES_PER_BUFFER * 2];
    size_t played = 0;
    while (played < sample_count) {
        size_t frame_count = sample_count - played;
        if (frame_count > SPEAKER_FRAMES_PER_BUFFER) {
            frame_count = SPEAKER_FRAMES_PER_BUFFER;
        }

        for (size_t index = 0; index < frame_count; ++index) {
            int16_t source_sample = (int16_t) read_u16_le(source + (played + index) * 2);
            int32_t scaled = (int32_t) source_sample *
                             SPEAKER_MAX_VOLUME_PERCENT / 100;
            int16_t sample = (int16_t) scaled;
            frames[index * 2] = sample;
            frames[index * 2 + 1] = sample;
        }

        err = write_frames(channel, frames, frame_count);
        if (err != ESP_OK) {
            return err;
        }
        played += frame_count;
    }
    return ESP_OK;
}

static esp_err_t play_sound(bool self_test, tokki_speaker_sound_t sound)
{
    i2s_chan_handle_t channel = NULL;
    bool enabled = false;
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    channel_config.auto_clear = true;

    esp_err_t err = i2s_new_channel(&channel_config, &channel, NULL);
    if (err != ESP_OK) {
        return err;
    }

    i2s_std_config_t standard_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SPEAKER_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = TOKKI_BOARD_SPEAKER_BCLK_GPIO,
            .ws = TOKKI_BOARD_SPEAKER_WS_GPIO,
            .dout = TOKKI_BOARD_SPEAKER_DATA_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(channel, &standard_config);
    if (err == ESP_OK) {
        err = i2s_channel_enable(channel);
        enabled = err == ESP_OK;
    }
    if (err == ESP_OK) {
        err = write_silence(channel, SPEAKER_SILENCE_DURATION_MS);
    }

    static const unsigned int test_frequencies[] = {440, 660, 880};
        for (size_t index = 0;
            self_test && err == ESP_OK &&
            index < sizeof(test_frequencies) / sizeof(test_frequencies[0]);
            ++index) {
           err = write_tone(channel, test_frequencies[index], test_frequencies[index],
                         SPEAKER_TONE_DURATION_MS);
        if (err == ESP_OK) {
            err = write_silence(channel, SPEAKER_SILENCE_DURATION_MS);
        }
    }

    if (err == ESP_OK) {
        if (self_test) {
            err = write_speech(channel, hello_ram_wav_start,
                                hello_ram_wav_end - hello_ram_wav_start);
        } else if (sound == TOKKI_SPEAKER_DRINK_WATER) {
            err = write_speech(channel, drink_water_wav_start,
                                drink_water_wav_end - drink_water_wav_start);
        } else {
            size_t count = 0;
            const speaker_tone_step_t *steps = speaker_sound_steps(sound, &count);
            if (steps == NULL) {
                err = ESP_ERR_INVALID_ARG;
            }
            for (size_t index = 0; err == ESP_OK && index < count; ++index) {
                err = write_tone(channel, steps[index].start_hz,
                                  steps[index].end_hz, steps[index].duration_ms);
                if (err == ESP_OK && steps[index].silence_ms > 0) {
                    err = write_silence(channel, steps[index].silence_ms);
                }
            }
        }
    }
    if (err == ESP_OK) {
        err = write_silence(channel, SPEAKER_SILENCE_DURATION_MS);
    }

    if (channel != NULL) {
        if (enabled) {
            esp_err_t disable_err = i2s_channel_disable(channel);
            if (err == ESP_OK && disable_err != ESP_OK) {
                err = disable_err;
            }
        }
        esp_err_t delete_err = i2s_del_channel(channel);
        if (err == ESP_OK && delete_err != ESP_OK) {
            err = delete_err;
        }
    }
    return err;
}

esp_err_t speaker_test_run(void)
{
    return play_sound(true, TOKKI_SPEAKER_ALERT);
}

esp_err_t tokki_speaker_play(tokki_speaker_sound_t sound)
{
    if (sound < TOKKI_SPEAKER_DRINK_WATER || sound > TOKKI_SPEAKER_PING) {
        return ESP_ERR_INVALID_ARG;
    }
    return play_sound(false, sound);
}
