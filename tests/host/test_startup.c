#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "tokki_board.h"
#include "tokki_gestures.h"
#include "tokki_led.h"
#include "tokki_neopixel.h"
#include "tokki_runtime.h"

void app_main(void);

static unsigned failing_stage;
static unsigned stage;
static unsigned latch_calls;
static unsigned runtime_calls;
static bool runtime_ready;
static bool indicator_fails;
static unsigned indicator_errors;

void test_log(const char *tag, const char *format, ...)
{
    (void) tag;
    if (format[0] == 'C') {
        ++indicator_errors;
    }
}

const char *esp_err_to_name(esp_err_t result)
{
    return result == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

static esp_err_t initialize(void)
{
    return ++stage == failing_stage ? ESP_FAIL : ESP_OK;
}

esp_err_t tokki_board_init(void) { return initialize(); }
esp_err_t tokki_led_init(void) { return initialize(); }
esp_err_t tokki_neopixel_init(void) { return initialize(); }

esp_err_t tokki_neopixel_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    assert(red == 0 && green == 0 && blue == 0);
    return initialize();
}

esp_err_t tokki_led_latch_failure(void)
{
    ++latch_calls;
    return indicator_fails ? ESP_FAIL : ESP_OK;
}

esp_err_t tokki_runtime_start(bool ready)
{
    ++runtime_calls;
    runtime_ready = ready;
    assert(ready || latch_calls == 1);
    return initialize();
}

size_t tokki_action_count(void) { return 0; }
const tokki_action_descriptor_t *tokki_action_at(size_t index)
{
    (void) index;
    return NULL;
}
const char *tokki_device_name(tokki_device_t device)
{
    (void) device;
    return "unused";
}

int main(void)
{
    for (unsigned failure = 0; failure <= 5; ++failure) {
        for (unsigned broken_led = 0; broken_led < 2; ++broken_led) {
            failing_stage = failure;
            indicator_fails = broken_led != 0;
            stage = latch_calls = runtime_calls = indicator_errors = 0;
            app_main();
            assert(stage == 5 && runtime_calls == 1);
            assert(runtime_ready == (failure == 0 || failure == 5));
            assert(latch_calls == (failure == 0 ? 0U : 1U));
            assert(indicator_errors == (indicator_fails ? latch_calls : 0));
        }
    }
    puts("PASS: startup failure LED for board/LED/RGB/initial write/runtime and indicator error logging");
    return 0;
}
