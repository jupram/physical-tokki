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
    (void) red;
    (void) green;
    (void) blue;
    return ESP_OK;
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
        }
        assert((size_t) cJSON_GetObjectItemCaseSensitive(result, "total")->valueint == tokki_action_count());
        cJSON *next = cJSON_GetObjectItemCaseSensitive(result, "nextCursor");
        assert(discovered < tokki_action_count() ? (size_t) next->valueint == discovered : cJSON_IsNull(next));
        cJSON_Delete(value);
    } while (discovered < tokki_action_count());
    assert(discovered == tokki_action_count());
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
    protocol.ready = false;
    feed(&protocol, "TOKKI/1 {\"id\":\"bad\",\"method\":\"action.run\",\"params\":{\"actionId\":\"oled.blink\"}}\n");
    expect_error(output_count - 1, "internal_error");
    assert(protocol.count == 0);
}

static void test_idle(void)
{
    tokki_idle_t idle;
    tokki_idle_reset(&idle);
    draw_result = ESP_OK;
    uint8_t centered[TOKKI_OLED_FRAME_SIZE];
    pet_eyes_render(centered, sizeof(centered), TOKKI_OLED_WIDTH, TOKKI_OLED_HEIGHT, PET_EYES_HAPPY, 0);
    for (unsigned index = 0; index < 297; ++index) {
        unsigned before = draw_calls;
        assert(tokki_idle_step(&idle) == ESP_OK);
        assert(draw_calls == before + 1);
        assert(delay_calls == 0);
        if (index == 0) {
            assert(memcmp(centered, last_frame, sizeof(centered)) == 0);
        }
        if (index == 100 || index == 175) {
            assert(memcmp(centered, last_frame, sizeof(centered)) != 0);
        }
    }
    assert(idle.phase == 0 && idle.frame == 0);
    draw_result = ESP_FAIL;
    assert(tokki_idle_step(&idle) == ESP_FAIL);
    assert(idle.phase == 0 && idle.frame == 0);
    tokki_protocol_t protocol;
    init(&protocol, true);
    tokki_protocol_idle_error(&protocol, ESP_FAIL);
    expect_event(0, "idle.error", NULL);
    /* A failed idle OLED does not prevent a queued LED action. */
    feed(&protocol, "TOKKI/1 {\"id\":\"led\",\"method\":\"action.run\",\"params\":{\"actionId\":\"led.blink\"}}\n");
    tokki_job_t job;
    assert(tokki_protocol_start_next_for_device(&protocol, TOKKI_DEVICE_LED, &job));
    assert(tokki_action_run(job.action_id) == ESP_OK);
    tokki_idle_reset(&idle);
    draw_result = ESP_OK;
    assert(tokki_idle_step(&idle) == ESP_OK);
    assert(memcmp(centered, last_frame, sizeof(centered)) == 0);
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
    test_idle();
    puts("PASS: serial framing, bounded JSON, catalog pagination, per-device FIFO/backpressure, lifecycle and idle frames");
    return 0;
}
