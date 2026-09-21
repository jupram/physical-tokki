#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "freertos/task.h"
#include "pet_eyes.h"
#include "tokki_gestures.h"
#include "tokki_idle.h"
#include "tokki_led.h"
#include "tokki_neopixel.h"
#include "tokki_oled.h"
#include "tokki_protocol.h"
#include "tokki_speaker.h"

static char output[64][TOKKI_FRAME_MAX + 1];
static size_t output_count;
static unsigned draw_calls;
static unsigned delay_calls;
static esp_err_t draw_result;
static esp_err_t color_result;
static unsigned color_calls;
static uint8_t last_color[3];
static uint8_t last_frame[TOKKI_OLED_FRAME_SIZE];

const char *esp_err_to_name(esp_err_t result)
{
    return result == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

esp_err_t tokki_oled_draw_frame(const uint8_t *frame, size_t length)
{
    assert(length == sizeof(last_frame));
    memcpy(last_frame, frame, length);
    ++draw_calls;
    return draw_result;
}

void vTaskDelay(TickType_t ticks)
{
    (void) ticks;
    ++delay_calls;
}

esp_err_t tokki_led_blink(uint32_t count, uint32_t on_ms, uint32_t off_ms)
{
    (void) count;
    (void) on_ms;
    (void) off_ms;
    return ESP_OK;
}

esp_err_t tokki_neopixel_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    last_color[0] = red;
    last_color[1] = green;
    last_color[2] = blue;
    ++color_calls;
    return color_result;
}

esp_err_t tokki_speaker_play(tokki_speaker_sound_t sound)
{
    (void) sound;
    return ESP_OK;
}

static void capture(const char *frame, size_t length, void *context)
{
    (void) context;
    assert(output_count < 64);
    assert(length <= TOKKI_FRAME_MAX);
    assert(length > 9 && memcmp(frame, "TOKKI/1 ", 8) == 0 && frame[length - 1] == '\n');
    memcpy(output[output_count], frame, length);
    output[output_count++][length] = '\0';
}

static void init(tokki_protocol_t *protocol, bool ready)
{
    output_count = 0;
    tokki_protocol_init(protocol, ready, capture, NULL);
}

static void feed(tokki_protocol_t *protocol, const char *line)
{
    tokki_protocol_feed(protocol, line, strlen(line));
}

static cJSON *message(size_t index)
{
    assert(index < output_count);
    cJSON *value = cJSON_Parse(output[index] + 8);
    assert(value != NULL);
    return value;
}

static void expect_error(size_t index, const char *code)
{
    cJSON *value = message(index);
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(value, "ok")));
    cJSON *error = cJSON_GetObjectItemCaseSensitive(value, "error");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(error, "code")->valuestring, code) == 0);
    cJSON_Delete(value);
}

static void expect_accepted(size_t index)
{
    cJSON *value = message(index);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(value, "ok")));
    cJSON *result = cJSON_GetObjectItemCaseSensitive(value, "result");
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(result, "accepted")));
    cJSON_Delete(value);
}

static void expect_event(size_t index, const char *event, const char *id)
{
    cJSON *value = message(index);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(value, "event")->valuestring, event) == 0);
    cJSON *data = cJSON_GetObjectItemCaseSensitive(value, "data");
    if (id != NULL) {
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(data, "requestId")->valuestring, id) == 0);
    }
    cJSON_Delete(value);
}

static void test_framing_and_validation(void)
{
    tokki_protocol_t protocol;
    init(&protocol, true);
    feed(&protocol, "boot log\nI (50) driver ready\r\n");
    assert(output_count == 0);
    const char *hello = "TOKKI/1 {\"id\":\"hello-1\",\"method\":\"hello\",\"params\":{}}\r\n";
    for (size_t index = 0; index < strlen(hello); ++index) {
        tokki_protocol_feed(&protocol, &hello[index], 1);
    }
    cJSON *value = message(0);
    cJSON *result = cJSON_GetObjectItemCaseSensitive(value, "result");
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(value, "ok")));
    assert(cJSON_GetObjectItemCaseSensitive(result, "protocol")->valueint == 1);
    assert(cJSON_GetObjectItemCaseSensitive(result, "queueCapacity")->valueint == 4);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(result, "ready")));
    cJSON_Delete(value);

    feed(&protocol, "TOKKI/1 not-json\nTOKKI/1 {}\n");
    expect_error(1, "invalid_request");
    expect_error(2, "invalid_request");
    feed(&protocol, "TOKKI/1 {\"id\":\"3\",\"method\":\"unknown\",\"params\":{}}\n");
    expect_error(3, "method_not_found");
    feed(&protocol, "TOKKI/1 {\"id\":\"4\",\"method\":\"hello\",\"params\":{}} garbage\n");
    expect_error(4, "invalid_request");
    feed(&protocol, "TOKKI/1 {\"id\":\"x\\u0000y\",\"method\":\"hello\",\"params\":{}}\n");
    expect_error(5, "invalid_request");
    feed(&protocol, "TOKKI/1 [[[[[[[[[0]]]]]]]]]\n");
    expect_error(6, "invalid_request");

    char oversized[TOKKI_FRAME_MAX + 10];
    memset(oversized, 'x', sizeof(oversized));
    memcpy(oversized, "TOKKI/1 ", 8);
    tokki_protocol_feed(&protocol, oversized, sizeof(oversized));
    feed(&protocol, "rest of discarded line\n");
    expect_error(7, "invalid_request");
    feed(&protocol, hello);
    assert(output_count == 9);

    char boundary[TOKKI_FRAME_MAX + 1];
    const char *valid = "TOKKI/1 {\"id\":\"boundary\",\"method\":\"hello\",\"params\":{}}";
    size_t length = strlen(valid);
    memcpy(boundary, valid, length);
    memset(boundary + length, ' ', TOKKI_FRAME_MAX - length - 1);
    boundary[TOKKI_FRAME_MAX - 1] = '\n';
    boundary[TOKKI_FRAME_MAX] = '\0';
    feed(&protocol, boundary);
    value = message(9);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(value, "ok")));
    cJSON_Delete(value);
    memcpy(boundary, "TOKKI/1 \0bad", 12);
    tokki_protocol_feed(&protocol, boundary, 12);
    feed(&protocol, "\n");
    expect_error(10, "invalid_request");
}

static void test_catalog(void)
{
    tokki_protocol_t protocol;
    init(&protocol, true);
    size_t discovered = 0;
    unsigned affectionate_eyes = 0;
    unsigned reference_tones = 0;
    unsigned sky_scenes = 0;
    unsigned sleeping_eyes = 0;
    unsigned scrolling_text = 0;
    do {
        char request[160];
        snprintf(request, sizeof(request),
                 "TOKKI/1 {\"id\":\"catalog\",\"method\":\"actions.list\",\"params\":{\"cursor\":%u}}\n",
                 (unsigned) discovered);
        feed(&protocol, request);
        cJSON *value = message(output_count - 1);
        cJSON *result = cJSON_GetObjectItemCaseSensitive(value, "result");
        assert(result != NULL);
        cJSON *actions = cJSON_GetObjectItemCaseSensitive(result, "actions");
        int count = cJSON_GetArraySize(actions);
        assert(count > 0 && count <= TOKKI_CATALOG_PAGE_SIZE);
        for (int index = 0; index < count; ++index) {
            cJSON *action = cJSON_GetArrayItem(actions, index);
            const tokki_action_descriptor_t *expected = tokki_action_at(discovered++);
            assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "id")->valuestring, expected->id) == 0);
            assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "name")->valuestring, expected->display_name) == 0);
            assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(action, "cancellable")));
            if (strcmp(expected->id, "oled.lovey_dovey") == 0 || strcmp(expected->id, "oled.shy") == 0) {
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "device")->valuestring, "oled") == 0);
                ++affectionate_eyes;
            }
            if (strncmp(expected->id, "speaker.tone_", 13) == 0) {
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "device")->valuestring, "speaker") == 0);
                ++reference_tones;
            }
            if (strcmp(expected->id, "oled.night_sky") == 0 || strcmp(expected->id, "oled.sunrise") == 0) {
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "device")->valuestring, "oled") == 0);
                ++sky_scenes;
            }
            if (strcmp(expected->id, "oled.sleeping") == 0) {
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "device")->valuestring, "oled") == 0);
                ++sleeping_eyes;
            }
            if (strcmp(expected->id, "oled.scrolling_text") == 0) {
                assert(discovered == 48);
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "name")->valuestring, "Scrolling text") == 0);
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(action, "device")->valuestring, "oled") == 0);
                ++scrolling_text;
            }
        }
        assert((size_t) cJSON_GetObjectItemCaseSensitive(result, "total")->valueint == tokki_action_count());
        cJSON *next = cJSON_GetObjectItemCaseSensitive(result, "nextCursor");
        assert(discovered < tokki_action_count() ? (size_t) next->valueint == discovered : cJSON_IsNull(next));
        cJSON_Delete(value);
    } while (discovered < tokki_action_count());
    assert(discovered == tokki_action_count());
    assert(affectionate_eyes == 2);
    assert(reference_tones == 4);
    assert(sky_scenes == 2);
    assert(sleeping_eyes == 1);
    assert(scrolling_text == 1 && discovered == 48);
    assert(output_count == (discovered + TOKKI_CATALOG_PAGE_SIZE - 1) / TOKKI_CATALOG_PAGE_SIZE);

    char beyond_catalog[32];
    snprintf(beyond_catalog, sizeof(beyond_catalog), "%u", (unsigned) tokki_action_count() + 1);
    const char *invalid[] = {"-1", "0.5", beyond_catalog, "\"0\"", "null"};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        char request[160];
        snprintf(request, sizeof(request),
                 "TOKKI/1 {\"id\":\"bad\",\"method\":\"actions.list\",\"params\":{\"cursor\":%s}}\n",
                 invalid[index]);
        feed(&protocol, request);
        expect_error(output_count - 1, "invalid_params");
    }
}

static void test_queue_and_lifecycle(void)
{
    tokki_protocol_t protocol;
    tokki_job_t job;
    init(&protocol, true);
    assert(!tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    for (unsigned index = 0; index < 5; ++index) {
        char request[160];
        snprintf(request, sizeof(request),
                 "TOKKI/1 {\"id\":\"run-%u\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.blink\"}}\n", index);
        feed(&protocol, request);
    }
    assert(protocol.count == 4);
    expect_error(4, "device_busy");
    for (unsigned index = 0; index < 4; ++index) {
        cJSON *value = message(index);
        assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(value, "ok")));
        cJSON_Delete(value);
    }
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    expect_event(5, "action.started", "run-0");
    assert(strcmp(job.action_id, "oled.blink") == 0 && protocol.count == 3);
    /* Reception and discovery continue while a job is active. */
    feed(&protocol, "TOKKI/1 {\"id\":\"extra\",\"method\":\"action.run\",\"params\":{\"actionId\":\"led.blink\"}}\n");
    feed(&protocol, "TOKKI/1 {\"id\":\"live\",\"method\":\"hello\",\"params\":{}}\n");
    assert(protocol.count == 4);
    /* A second device can start while the first OLED job is still active. */
    tokki_job_t parallel_job;
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_LED, &parallel_job));
    assert(strcmp(parallel_job.request_id, "extra") == 0);
    tokki_protocol_finish(&protocol, &parallel_job, ESP_OK);
    assert(!tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_LED, &parallel_job));
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    expect_event(10, "action.completed", "run-0");
    for (unsigned index = 1; index < 4; ++index) {
        assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
        char expected[16];
        snprintf(expected, sizeof(expected), "run-%u", index);
        assert(strcmp(job.request_id, expected) == 0);
        tokki_protocol_finish(&protocol, &job, ESP_FAIL);
        expect_event(output_count - 1, "action.failed", expected);
    }
    assert(!tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    feed(&protocol, "TOKKI/1 {\"id\":\"stop\",\"method\":\"action.stop\",\"params\":{\"requestId\":\"run-0\"}}\n");
    expect_error(output_count - 1, "not_cancellable");
    feed(&protocol, "TOKKI/1 {\"id\":\"stop\",\"method\":\"action.stop\",\"params\":{}}\n");
    expect_error(output_count - 1, "invalid_params");
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.nope\"}}\n");
    expect_error(output_count - 1, "action_not_found");
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.blink\",\"text\":\"oops\"}}\n");
    expect_error(output_count - 1, "invalid_params");
    feed(&protocol, "TOKKI/1 {\"id\":\"marquee\",\"method\":\"oled.marquee\",\"params\":{\"text\":\"Teams: Build 42!\"}}\n");
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    assert(strcmp(job.request_id, "marquee") == 0);
    assert(strcmp(job.action_id, "oled.marquee") == 0);
    assert(strcmp(job.text, "Teams: Build 42!") == 0);
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    feed(&protocol, "TOKKI/1 {\"id\":\"light\",\"method\":\"neopixel.notification\",\"params\":{\"text\":\"Teams: Build 42!\",\"color\":\"purple\"}}\n");
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_NEOPIXEL, &job));
    assert(strcmp(job.request_id, "light") == 0);
    assert(strcmp(job.action_id, TOKKI_NOTIFICATION_LIGHT_PURPLE_ACTION_ID) == 0);
    assert(strcmp(job.text, "Teams: Build 42!") == 0);
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"neopixel.notification\",\"params\":{\"text\":\"ok\",\"color\":\"red\"}}\n");
    expect_error(output_count - 1, "invalid_params");
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"oled.marquee\",\"params\":{\"text\":\"\"}}\n");
    expect_error(output_count - 1, "invalid_params");
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"oled.marquee\",\"params\":{\"text\":\"123456789012345678901234567890123456789012345678901\"}}\n");
    expect_error(output_count - 1, "invalid_params");
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"oled.marquee\",\"params\":{\"text\":\"ok\",\"extra\":true}}\n");
    expect_error(output_count - 1, "invalid_params");
    protocol.ready = false;
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.blink\"}}\n");
    expect_error(output_count - 1, "internal_error");
    assert(protocol.count == 0);
}

static void test_scrolling_text_protocol(void)
{
    tokki_protocol_t protocol;
    tokki_job_t job;
    init(&protocol, true);
    feed(&protocol, "TOKKI/1 {\"id\":\"eyes\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.curious\"}}\n");
    feed(&protocol, "TOKKI/1 {\"id\":\"title\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\",\"text\":\"12345678901234567890123456789012345678901234567890\"}}\n");
    feed(&protocol, "TOKKI/1 {\"id\":\"sound\",\"method\":\"action.run\",\"params\":{\"actionId\":\"speaker.trill\"}}\n");
    feed(&protocol, "TOKKI/1 {\"id\":\"manual\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\"}}\n");
    assert(protocol.count == 4 && output_count == 4);
    for (size_t index = 0; index < 4; ++index) expect_accepted(index);
    feed(&protocol, "TOKKI/1 {\"id\":\"full\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\",\"text\":\"one more\"}}\n");
    expect_error(4, "device_busy");
    assert(protocol.count == 4);

    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_SPEAKER, &job));
    assert(strcmp(job.request_id, "sound") == 0 && job.text[0] == '\0');
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    assert(strcmp(job.request_id, "eyes") == 0 && job.text[0] == '\0');
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    expect_event(output_count - 1, "action.completed", "eyes");
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    expect_event(output_count - 1, "action.started", "title");
    assert(strcmp(job.action_id, "oled.scrolling_text") == 0);
    assert(strlen(job.text) == 50 && strcmp(job.text, "12345678901234567890123456789012345678901234567890") == 0);
    tokki_protocol_finish(&protocol, &job, ESP_FAIL);
    expect_event(output_count - 1, "action.failed", "title");
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    assert(strcmp(job.request_id, "manual") == 0 && strcmp(job.text, "Hello from Tokki!") == 0);
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    expect_event(output_count - 1, "action.completed", "manual");
    assert(protocol.count == 0);

    const char *invalid_params[] = {
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"123456789012345678901234567890123456789012345678901\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":null}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":42}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":true}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":[]}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":{}}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"line\\nfeed\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"\\t\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"\\u001f\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"\\u007f\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"\\u00e9\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"ok\",\"extra\":true}",
        "{\"actionId\":\"oled.scrolling_text\",\"extra\":true}",
        "{\"actionId\":\"oled.scrolling_text\",\"text\":\"ok\",\"text\":\"duplicate\"}",
        "{\"actionId\":\"oled.scrolling_text\",\"actionId\":\"oled.scrolling_text\"}",
        "{\"actionId\":\"oled.curious\",\"text\":\"not allowed\"}",
        "{\"actionId\":\"oled.marquee\",\"text\":\"not a catalog action\"}",
        "{\"actionId\":\"speaker.trill\",\"text\":\"not allowed\"}",
        "{\"actionId\":\"oled.nope\",\"text\":\"not allowed\"}",
        "{\"text\":\"missing actionId\"}",
    };
    for (size_t index = 0; index < sizeof(invalid_params) / sizeof(invalid_params[0]); ++index) {
        char request[300];
        snprintf(request, sizeof(request),
                 "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":%s}\n", invalid_params[index]);
        feed(&protocol, request);
        expect_error(output_count - 1, "invalid_params");
        assert(protocol.count == 0);
    }
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\",\"text\":\"bad\\u0000text\"}}\n");
    expect_error(output_count - 1, "invalid_request");
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\",\"text\":}}\n");
    expect_error(output_count - 1, "invalid_request");
    feed(&protocol, "TOKKI/1 {\"id\":\"space\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\",\"text\":\" \"}}\n");
    expect_accepted(output_count - 1);
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    assert(strcmp(job.text, " ") == 0);
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    feed(&protocol, "TOKKI/1 {\"id\":\"escaped\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\",\"text\":\" ~\\\"\\\\\"}}\n");
    expect_accepted(output_count - 1);
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    assert(strcmp(job.text, " ~\"\\") == 0);
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    protocol.ready = false;
    feed(&protocol, "TOKKI/1 {\"id\":\"not-ready\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.scrolling_text\"}}\n");
    expect_error(output_count - 1, "internal_error");
    assert(protocol.count == 0);
}

static void test_legacy_text_limits(void)
{
    const char *methods[] = {"oled.marquee", "neopixel.notification"};
    for (unsigned method = 0; method < 2; ++method) {
        tokki_protocol_t protocol;
        tokki_job_t job;
        init(&protocol, true);
        for (unsigned length = 40; length <= 51; ++length) {
            char text[52];
            memset(text, 'A', length);
            text[length] = '\0';
            char request[256];
            snprintf(request, sizeof(request),
                     "TOKKI/1 {\"id\":\"legacy\",\"method\":\"%s\",\"params\":{\"text\":\"%s\"%s}}\n",
                     methods[method], text, method == 1 ? ",\"color\":\"blue\"" : "");
            feed(&protocol, request);
            if (length == 51) {
                expect_error(output_count - 1, "invalid_params");
                assert(protocol.count == 0);
                continue;
            }
            expect_accepted(output_count - 1);
            assert(tokki_protocol_start_next_for_device(&protocol,
                   method == 0 ? TOKKI_DEVICE_OLED : TOKKI_DEVICE_NEOPIXEL, &job));
            assert(strcmp(job.text, text) == 0);
            assert(strcmp(job.action_id, method == 0 ? "oled.marquee" : TOKKI_NOTIFICATION_LIGHT_BLUE_ACTION_ID) == 0);
            tokki_protocol_finish(&protocol, &job, ESP_OK);
            expect_event(output_count - 1, "action.completed", "legacy");
        }
    }
}

static void expect_idle_frame(tokki_idle_t *idle, pet_eye_expression_t expression, unsigned frame)
{
    uint8_t expected[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(expected, sizeof(expected), TOKKI_OLED_WIDTH, TOKKI_OLED_HEIGHT, expression, frame);
    unsigned before = draw_calls;
    assert(tokki_idle_step(idle) == ESP_OK);
    assert(draw_calls == before + 1 && delay_calls == 0);
    assert(memcmp(expected, last_frame, sizeof(expected)) == 0);
}

static void test_idle(void)
{
    const pet_eye_expression_t expressions[] = {
        PET_EYES_HAPPY, PET_EYES_LOOK_LEFT, PET_EYES_LOOK_RIGHT, PET_EYES_LOOK_UP,
        PET_EYES_HAPPY, PET_EYES_CURIOUS, PET_EYES_LOVEY_DOVEY, PET_EYES_SHY,
        PET_EYES_SLEEPING,
    };
    const unsigned frames[] = {9, 24, 24, 24, 48, 48, 48, 48, 96};
    assert(TOKKI_IDLE_EYE_VARIANTS == 9);
    unsigned first_choices = 0;
    unsigned minimum_rest = 20;
    unsigned maximum_rest = 10;
    tokki_idle_t idle;
    draw_result = ESP_OK;
    for (uint32_t seed = 0; seed < 8; ++seed) {
        tokki_idle_reset(&idle, seed);
        unsigned previous = TOKKI_IDLE_EYE_VARIANTS;
        for (unsigned round = 0; round < 2; ++round) {
            unsigned seen = 0;
            unsigned glance_blinks = 0;
            for (unsigned choice = 0; choice < TOKKI_IDLE_EYE_VARIANTS; ++choice) {
                assert(idle.resting && idle.frame == 0);
                unsigned rest = idle.frames;
                assert(rest * TOKKI_IDLE_FRAME_MS >= 600 && rest * TOKKI_IDLE_FRAME_MS <= 1200);
                if (rest < minimum_rest) minimum_rest = rest;
                if (rest > maximum_rest) maximum_rest = rest;
                for (unsigned frame = 0; frame < rest; ++frame) {
                    expect_idle_frame(&idle, PET_EYES_HAPPY, 0);
                }
                assert(!idle.resting && idle.frame == 0);
                unsigned phase = idle.phase;
                assert(phase < TOKKI_IDLE_EYE_VARIANTS && phase != previous);
                assert((seen & (1U << phase)) == 0);
                bool side_glance = expressions[phase] == PET_EYES_LOOK_LEFT ||
                                   expressions[phase] == PET_EYES_LOOK_RIGHT;
                assert(idle.frames == frames[phase] + (side_glance ? 9 : 0));
                if (round == 0 && choice == 0) first_choices |= 1U << phase;
                seen |= 1U << phase;
                previous = phase;
                unsigned sequence_next_phase = idle.next_phase;
                uint32_t sequence_random_state = idle.random_state;
                for (unsigned frame = 0; frame < frames[phase]; ++frame) {
                    bool restore = frame + 1 == frames[phase];
                    if (phase == 8) {
                        assert(!idle.resting && idle.phase == 8 && idle.frame == frame);
                        assert(idle.next_phase == sequence_next_phase &&
                               idle.random_state == sequence_random_state);
                        tokki_idle_t snapshot;
                        memcpy(&snapshot, &idle, sizeof(idle));
                        draw_result = ESP_FAIL;
                        assert(tokki_idle_step(&idle) == ESP_FAIL);
                        assert(memcmp(&snapshot, &idle, sizeof(idle)) == 0);
                        draw_result = ESP_OK;
                    }
                    if (phase == 8 && frame >= 48 && !restore) {
                        uint8_t expected[TOKKI_OLED_FRAME_SIZE];
                        assert(tokki_oled_render_art(expected, sizeof(expected),
                                                      TOKKI_OLED_ART_NIGHT_SKY, frame - 48) == ESP_OK);
                        unsigned before = draw_calls;
                        assert(tokki_idle_step(&idle) == ESP_OK);
                        assert(draw_calls == before + 1 && delay_calls == 0);
                        assert(memcmp(expected, last_frame, sizeof(expected)) == 0);
                    } else {
                        unsigned eye_frame = phase == 8 && frame > 39 ? 39 :
                                             (phase == 0 ? 34 : 0) + frame;
                        expect_idle_frame(&idle, restore ? PET_EYES_HAPPY : expressions[phase],
                                          restore ? 0 : eye_frame);
                    }
                }
                if (side_glance) {
                    assert(!idle.resting && idle.frame == 24);
                    unsigned next_phase = idle.next_phase;
                    uint32_t random_state = idle.random_state;
                    for (unsigned frame = 0; frame < 9; ++frame) {
                        assert(!idle.resting && idle.phase == phase);
                        assert(idle.next_phase == next_phase && idle.random_state == random_state);
                        if (frame == 4) {
                            tokki_idle_t snapshot;
                            memcpy(&snapshot, &idle, sizeof(idle));
                            draw_result = ESP_FAIL;
                            assert(tokki_idle_step(&idle) == ESP_FAIL);
                            assert(memcmp(&snapshot, &idle, sizeof(idle)) == 0);
                            draw_result = ESP_OK;
                        }
                        expect_idle_frame(&idle, PET_EYES_HAPPY, 34 + frame);
                    }
                    ++glance_blinks;
                }
                assert(idle.resting && idle.frame == 0);
            }
            assert(seen == (1U << TOKKI_IDLE_EYE_VARIANTS) - 1 && glance_blinks == 2);
        }
    }
    assert((first_choices & (first_choices - 1)) != 0);
    assert(minimum_rest == 10 && maximum_rest == 20);

    tokki_idle_reset(&idle, 123);
    tokki_idle_t same;
    tokki_idle_reset(&same, 123);
    unsigned rest = idle.frames;
    for (unsigned frame = 0; frame < rest; ++frame) {
        assert(tokki_idle_step(&idle) == ESP_OK);
        assert(tokki_idle_step(&same) == ESP_OK);
    }
    assert(idle.phase == same.phase && idle.random_state == same.random_state);
    tokki_idle_t snapshot;
    memcpy(&snapshot, &idle, sizeof(idle));
    draw_result = ESP_FAIL;
    assert(tokki_idle_step(&idle) == ESP_FAIL);
    assert(memcmp(&idle, &snapshot, sizeof(idle)) == 0);
    tokki_protocol_t protocol;
    init(&protocol, true);
    tokki_protocol_idle_error(&protocol, ESP_FAIL);
    expect_event(0, "idle.error", NULL);
    /* A failed idle OLED does not prevent a queued LED action. */
    feed(&protocol, "TOKKI/1 {\"id\":\"led\",\"method\":\"action.run\",\"params\":{\"actionId\":\"led.blink\"}}\n");
    tokki_job_t job;
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_LED, &job));
    assert(tokki_action_run(job.action_id) == ESP_OK);
    tokki_idle_reset(&idle, 456);
    draw_result = ESP_OK;
    expect_idle_frame(&idle, PET_EYES_HAPPY, 0);
}

static void test_idle_neopixel(void)
{
    tokki_idle_neopixel_t idle;
    tokki_idle_neopixel_reset(&idle, 0);
    color_result = ESP_OK;
    unsigned previous_pause = 0;
    bool varied = false;
    for (unsigned burst = 0; burst < 32; ++burst) {
        assert(idle.resting);
        unsigned before = color_calls;
        assert(tokki_idle_neopixel_step(&idle) == ESP_OK);
        assert(color_calls == before + 1 && delay_calls == 0);
        assert(last_color[0] == 0 && last_color[1] == 0 && last_color[2] == 0);
        assert(idle.delay_ms >= 6000 && idle.delay_ms <= 14000);
        if (previous_pause != 0 && previous_pause != idle.delay_ms) varied = true;
        previous_pause = idle.delay_ms;
        assert(!idle.resting && idle.frame == 0);
        unsigned duration = 0;
        for (unsigned frame = 0; frame < 33; ++frame) {
            tokki_idle_neopixel_t snapshot;
            memcpy(&snapshot, &idle, sizeof(idle));
            color_result = ESP_FAIL;
            assert(tokki_idle_neopixel_step(&idle) == ESP_FAIL);
            assert(memcmp(&idle, &snapshot, sizeof(idle)) == 0);
            color_result = ESP_OK;
            before = color_calls;
            assert(tokki_idle_neopixel_step(&idle) == ESP_OK);
            assert(color_calls == before + 1 && delay_calls == 0);
            unsigned brightness = 2 * (frame <= 16 ? frame : 32 - frame);
            assert(last_color[0] == 0 && last_color[1] == brightness && last_color[2] == brightness);
            assert(idle.delay_ms == 60);
            duration += idle.delay_ms;
        }
        assert(duration == 1980 && idle.resting);
    }
    assert(varied);
    tokki_idle_neopixel_reset(&idle, 42);
    tokki_idle_neopixel_t snapshot;
    memcpy(&snapshot, &idle, sizeof(idle));
    color_result = ESP_FAIL;
    assert(tokki_idle_neopixel_step(&idle) == ESP_FAIL);
    assert(memcmp(&idle, &snapshot, sizeof(idle)) == 0);
    color_result = ESP_OK;
    assert(tokki_idle_neopixel_step(&idle) == ESP_OK);
    assert(last_color[0] == 0 && last_color[1] == 0 && last_color[2] == 0);
}

static void export_wire_fixture(void)
{
    tokki_protocol_t protocol;
    init(&protocol, true);
    feed(&protocol, "TOKKI/1 {\"id\":\"fixture-hello\",\"method\":\"hello\",\"params\":{}}\n");
    for (size_t cursor = 0; cursor < tokki_action_count(); cursor += TOKKI_CATALOG_PAGE_SIZE) {
        char request[160];
        snprintf(request, sizeof(request),
                 "TOKKI/1 {\"id\":\"fixture-page-%u\",\"method\":\"actions.list\",\"params\":{\"cursor\":%u}}\n",
                 (unsigned) cursor, (unsigned) cursor);
        feed(&protocol, request);
    }
    feed(&protocol, "TOKKI/1 {\"id\":\"fixture-run\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.blink\"}}\n");
    tokki_job_t job;
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    tokki_protocol_finish(&protocol, &job, ESP_OK);
    feed(&protocol, "TOKKI/1 {\"id\":\"fixture-fail\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.blink\"}}\n");
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_OLED, &job));
    tokki_protocol_finish(&protocol, &job, ESP_FAIL);
    tokki_protocol_idle_error(&protocol, ESP_FAIL);
    feed(&protocol, "TOKKI/1 {\"id\":\"fixture-missing\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.missing\"}}\n");
    for (size_t index = 0; index < output_count; ++index) {
        fputs(output[index], stdout);
    }
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--wire-fixture") == 0) {
        export_wire_fixture();
        return 0;
    }
    test_framing_and_validation();
    test_catalog();
    test_queue_and_lifecycle();
    test_scrolling_text_protocol();
    test_legacy_text_limits();
    test_idle();
    test_idle_neopixel();
    puts("PASS: serial framing, catalog, per-device FIFO/lifecycle, shuffled idle eyes, intermittent teal breathing");
    return 0;
}
