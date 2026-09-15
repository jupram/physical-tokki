#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "freertos/task.h"
#include "tokki_gestures.h"
#include "tokki_led.h"
#include "tokki_neopixel.h"
#include "tokki_oled.h"
#include "pet_eyes.h"
#include "tokki_speaker.h"
#include "speaker_tone.h"

static unsigned color_calls;
static unsigned fail_color_call;
static unsigned elapsed_ms;
static uint8_t colors[8][3];
static esp_err_t rainbow_result;
static unsigned oled_calls;
static unsigned fail_oled_call;
static uint8_t last_frame[TOKKI_OLED_FRAME_SIZE];
static tokki_speaker_sound_t last_sound;
static esp_err_t speaker_result;
static FILE *preview_output;
static bool record_preview;
static unsigned preview_actions;

esp_err_t tokki_speaker_play(tokki_speaker_sound_t sound)
{
    last_sound = sound;
    return speaker_result;
}

esp_err_t tokki_oled_draw_frame(const uint8_t *framebuffer, size_t size)
{
    assert(framebuffer != NULL && size == TOKKI_OLED_FRAME_SIZE);
    size_t lit = 0;
    for (size_t index = 0; index < size; ++index) {
        lit += framebuffer[index] != 0;
    }
    assert(lit > 0 && lit < size);
    if (record_preview) {
        fprintf(preview_output, "%s\"", oled_calls == 0 ? "" : ",");
        for (size_t index = 0; index < size; ++index) {
            fprintf(preview_output, "%02x", framebuffer[index]);
        }
        fputc('"', preview_output);
    }
    memcpy(last_frame, framebuffer, size);
    ++oled_calls;
    return oled_calls == fail_oled_call ? ESP_FAIL : ESP_OK;
}

void vTaskDelay(TickType_t ticks)
{
    elapsed_ms += ticks;
}

esp_err_t tokki_neopixel_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    assert(color_calls < 8);
    colors[color_calls][0] = red;
    colors[color_calls][1] = green;
    colors[color_calls][2] = blue;
    ++color_calls;
    return color_calls == fail_color_call ? ESP_FAIL : ESP_OK;
}

esp_err_t tokki_neopixel_rainbow(uint32_t cycles)
{
    assert(cycles == 1);
    return rainbow_result;
}

esp_err_t tokki_led_blink(uint32_t count, uint32_t on_ms, uint32_t off_ms)
{
    assert(count == 3 && on_ms == 200 && off_ms == 200);
    return ESP_OK;
}

static void reset_output(void)
{
    color_calls = 0;
    fail_color_call = 0;
    elapsed_ms = 0;
    rainbow_result = ESP_OK;
    oled_calls = 0;
    fail_oled_call = 0;
    memset(colors, 0, sizeof(colors));
}

static void test_blink(const char *id, uint8_t red, uint8_t green)
{
    reset_output();
    assert(tokki_action_run(id) == ESP_OK);
    assert(color_calls == 6 && elapsed_ms == 1200);
    for (unsigned index = 0; index < 6; ++index) {
        assert(colors[index][0] == (index % 2 == 0 ? red : 0));
        assert(colors[index][1] == (index % 2 == 0 ? green : 0));
        assert(colors[index][2] == 0);
    }
    for (unsigned failure = 1; failure <= 6; ++failure) {
        reset_output();
        fail_color_call = failure;
        assert(tokki_action_run(id) == ESP_FAIL);
        assert(color_calls == failure);
        assert(elapsed_ms == (failure - 1) * 200);
    }
}

static void test_oled(const char *id, unsigned frames, unsigned duration)
{
    reset_output();
    if (preview_output != NULL) {
        fprintf(preview_output, "%s\"%s\":{\"duration\":%u,\"frames\":[",
                preview_actions++ == 0 ? "" : ",\n", id, duration);
        record_preview = true;
    }
    assert(tokki_action_run(id) == ESP_OK);
    record_preview = false;
    if (preview_output != NULL) {
        fputs("]}", preview_output);
    }
    assert(oled_calls == frames && elapsed_ms == duration);
    for (unsigned failure = 1; failure <= frames; ++failure) {
        reset_output();
        fail_oled_call = failure;
        assert(tokki_action_run(id) == ESP_FAIL);
        assert(oled_calls == failure);
        assert(elapsed_ms == (failure - 1) * (duration / frames));
    }
}

static void test_renderers(void)
{
    uint8_t guarded[TOKKI_OLED_FRAME_SIZE + 2];
    memset(guarded, 0xA5, sizeof(guarded));
    for (unsigned frame = 0; frame < 48; ++frame) {
        for (int expression = PET_EYES_HAPPY; expression <= PET_EYES_SURPRISED; ++expression) {
            pet_eyes_render(guarded + 1, TOKKI_OLED_FRAME_SIZE, 128, 64,
                             (pet_eye_expression_t) expression, frame);
            assert(guarded[0] == 0xA5 && guarded[sizeof(guarded) - 1] == 0xA5);
        }
        for (int art = TOKKI_OLED_ART_DRINK_WATER; art <= TOKKI_OLED_ART_FIRE; ++art) {
            assert(tokki_oled_render_art(guarded + 1, TOKKI_OLED_FRAME_SIZE,
                                          (tokki_oled_art_t) art, frame) == ESP_OK);
            assert(guarded[0] == 0xA5 && guarded[sizeof(guarded) - 1] == 0xA5);
        }
    }
    assert(tokki_oled_render_art(NULL, TOKKI_OLED_FRAME_SIZE, TOKKI_OLED_ART_FIRE, 0) == ESP_ERR_INVALID_ARG);
    assert(tokki_oled_render_art(guarded, 1, TOKKI_OLED_ART_FIRE, 0) == ESP_ERR_INVALID_ARG);
    assert(tokki_oled_render_art(guarded, TOKKI_OLED_FRAME_SIZE, (tokki_oled_art_t) 99, 0) == ESP_ERR_INVALID_ARG);
    uint8_t happy[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 0);
    pet_eyes_render(guarded + 1, TOKKI_OLED_FRAME_SIZE, 128, 64, PET_EYES_SURPRISED, 0);
    assert(memcmp(happy, guarded + 1, sizeof(happy)) != 0);
    reset_output();
    assert(tokki_action_run("oled.blink") == ESP_OK);
    pet_eyes_render(happy, sizeof(happy), 128, 64, PET_EYES_HAPPY, 42);
    assert(memcmp(happy, last_frame, sizeof(happy)) == 0);
}

static void test_speaker(void)
{
    const char *ids[] = {"speaker.drink_water", "speaker.chirp", "speaker.alert"};
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); ++index) {
        speaker_result = ESP_OK;
        assert(tokki_action_run(ids[index]) == ESP_OK);
        assert(last_sound == (tokki_speaker_sound_t) index);
        speaker_result = ESP_FAIL;
        assert(tokki_action_run(ids[index]) == ESP_FAIL);
    }
    for (unsigned frame = 0; frame < 4800; ++frame) {
        for (uint32_t phase_index = 0; phase_index < 32; ++phase_index) {
            int16_t sample = speaker_tone_sample(frame, 4800, phase_index << 27);
            assert(sample >= -6553 && sample <= 6553);
            if (frame == 0 || frame == 4799) {
                assert(sample == 0);
            }
        }
    }
    assert(speaker_tone_sample(0, 0, UINT32_MAX) == 0);
    assert(speaker_tone_sample(4800, 4800, UINT32_MAX) == 0);
    for (uint32_t phase_index = 0; phase_index < 16; ++phase_index) {
        assert(speaker_tone_sample(2400, 4800, phase_index << 27) ==
               -speaker_tone_sample(2400, 4800, (phase_index + 16) << 27));
    }
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        assert(fopen_s(&preview_output, argv[1], "w") == 0);
        fputs("window.TOKKI_PREVIEW = {\n", preview_output);
    }
    assert(tokki_action_count() == 16);
    assert(tokki_action_at(tokki_action_count()) == NULL);
    assert(tokki_action_find(NULL) == NULL);
    assert(tokki_action_run("missing") == ESP_ERR_NOT_FOUND);
    assert(tokki_action_run(NULL) == ESP_ERR_NOT_FOUND);
    for (size_t index = 0; index < tokki_action_count(); ++index) {
        const tokki_action_descriptor_t *action = tokki_action_at(index);
        assert(action != NULL && action->run != NULL);
        assert(action->display_name != NULL && !action->cancellable);
        assert(tokki_action_find(action->id) == action);
        const char *device = tokki_device_name(action->device);
        assert(strncmp(action->id, device, strlen(device)) == 0);
        assert(action->id[strlen(device)] == '.');
        for (size_t other = index + 1; other < tokki_action_count(); ++other) {
            assert(strcmp(action->id, tokki_action_at(other)->id) != 0);
        }
    }
    assert(tokki_action_run("led.blink") == ESP_OK);
    test_blink("neopixel.blink_red", 32, 0);
    test_blink("neopixel.blink_yellow", 32, 32);
    test_blink("neopixel.blink_green", 0, 32);
    reset_output();
    assert(tokki_action_run("neopixel.rainbow") == ESP_OK);
    assert(color_calls == 1 && colors[0][0] == 0 && colors[0][1] == 0 && colors[0][2] == 0);
    reset_output();
    rainbow_result = ESP_FAIL;
    assert(tokki_action_run("neopixel.rainbow") == ESP_FAIL);
    assert(color_calls == 0);
    reset_output();
    fail_color_call = 1;
    assert(tokki_action_run("neopixel.rainbow") == ESP_FAIL);
    test_oled("oled.happy", 48, 2880);
    test_oled("oled.sad", 48, 2880);
    test_oled("oled.surprised", 48, 2880);
    test_oled("oled.curious", 48, 2880);
    test_oled("oled.blink", 9, 540);
    test_oled("oled.drink_water", 1, 2000);
    test_oled("oled.water_drop", 24, 1440);
    test_oled("oled.fire", 24, 1440);
    test_renderers();
    test_speaker();
    if (preview_output != NULL) {
        fputs("\n};\n", preview_output);
        assert(fclose(preview_output) == 0);
    }
    puts("PASS: 16 actions, RGB timing/colors, OLED frames/bounds/timing, speaker routing/envelope/ceiling, driver failures");
    return 0;
}