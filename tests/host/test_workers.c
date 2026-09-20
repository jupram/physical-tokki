#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tokki_idle.h"
#include "tokki_led.h"
#include "tokki_neopixel.h"
#include "tokki_oled.h"
#include "tokki_protocol.h"
#include "tokki_runtime.h"

typedef struct {
    TaskFunction_t function;
    void *argument;
} captured_task_t;

static captured_task_t tasks[TOKKI_DEVICE_COUNT + 1];
static unsigned task_count;
static bool locked;
static bool started;
static bool progress_only;
static bool fail_idle;
static bool fail_action;
static bool pending[TOKKI_DEVICE_COUNT];
static tokki_device_t device;
static TickType_t now;
static TickType_t first_wait;
static TickType_t last_wait;
static TickType_t action_time;
static unsigned waits;
static unsigned wait_limit;
static unsigned lead_in_waits;
static unsigned writes_before_action;
static unsigned writes;
static unsigned actions;
static unsigned completions;
static unsigned faults;
static unsigned idle_errors;
static unsigned logs;
static TickType_t write_times[40];
static uint8_t colors[40];
static jmp_buf worker_done;

BaseType_t xTaskCreate(TaskFunction_t function, const char *name, uint32_t stack_depth,
                       void *argument, UBaseType_t priority, TaskHandle_t *handle)
{
    assert(name != NULL && stack_depth > 0 && priority > 0);
    assert(task_count < TOKKI_DEVICE_COUNT + 1);
    tasks[task_count] = (captured_task_t) {function, argument};
    *handle = &tasks[task_count++];
    return pdPASS;
}

void vTaskDelete(TaskHandle_t handle) { assert(handle != NULL); }
void xTaskNotifyGive(TaskHandle_t handle) { assert(handle != NULL); }

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &locked; }
void vSemaphoreDelete(SemaphoreHandle_t handle) { assert(handle == &locked); }

BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t timeout)
{
    assert(handle == &locked && !locked && timeout == portMAX_DELAY);
    locked = true;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t handle)
{
    assert(handle == &locked && locked);
    locked = false;
    return pdTRUE;
}

void vTaskSetTimeOutState(TimeOut_t *timeout) { timeout->started = now; }

BaseType_t xTaskCheckForTimeOut(TimeOut_t *timeout, TickType_t *remaining)
{
    TickType_t elapsed = now - timeout->started;
    timeout->started = now;
    if (elapsed >= *remaining) {
        *remaining = 0;
        return pdTRUE;
    }
    *remaining -= elapsed;
    return pdFALSE;
}

uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t timeout)
{
    assert(clear == pdTRUE && !locked);
    if (!started) {
        assert(timeout == portMAX_DELAY);
        started = true;
        return 1;
    }
    last_wait = timeout;
    if (lead_in_waits > 0) {
        --lead_in_waits;
        now += timeout;
        return 0;
    }
    if (waits == wait_limit) {
        longjmp(worker_done, 1);
    }
    if (waits == 0) {
        first_wait = timeout;
        writes_before_action = writes;
    }
    if (progress_only) {
        now += timeout;
        ++waits;
        return 0;
    }
    assert(writes == writes_before_action && actions == 0);
    if (waits == 0) {
        now += 10;
        pending[device == TOKKI_DEVICE_OLED ? TOKKI_DEVICE_NEOPIXEL : TOKKI_DEVICE_OLED] = true;
    } else if (waits == 1) {
        assert(timeout == first_wait - 10);
        now += 20;
    } else {
        assert(waits == 2 && timeout == first_wait - 30);
        now += 5;
        pending[device] = true;
    }
    ++waits;
    return 1;
}

void vTaskDelay(TickType_t ticks)
{
    (void) ticks;
    assert(false && "Idle playback must never use a blocking gesture runner");
}

esp_err_t uart_driver_install(int port, int rx_size, int tx_size, int queue_size, void *queue, int flags)
{
    (void) port; (void) rx_size; (void) tx_size; (void) queue_size; (void) queue; (void) flags;
    return ESP_OK;
}
esp_err_t uart_driver_delete(int port) { (void) port; return ESP_OK; }
int uart_read_bytes(int port, void *buffer, size_t length, TickType_t timeout)
{
    (void) port; (void) buffer; (void) length; (void) timeout;
    assert(false);
    return 0;
}

uint32_t esp_random(void) { return 12345; }
const char *esp_err_to_name(esp_err_t result) { return result == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }
void test_log(const char *tag, const char *format, ...)
{
    assert(tag != NULL && format != NULL);
    ++logs;
}
esp_err_t tokki_led_latch_failure(void) { ++faults; return ESP_OK; }

static esp_err_t record_write(void)
{
    assert(!locked && writes < 40);
    write_times[writes++] = now;
    return fail_idle && writes == 1 ? ESP_FAIL : ESP_OK;
}

esp_err_t tokki_oled_draw_frame(const uint8_t *frame, size_t length)
{
    assert(device == TOKKI_DEVICE_OLED && frame != NULL && length == TOKKI_OLED_FRAME_SIZE);
    return record_write();
}

esp_err_t tokki_neopixel_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    assert(device == TOKKI_DEVICE_NEOPIXEL && red == 0 && green == blue && blue <= 32);
    assert(writes < 40);
    colors[writes] = blue;
    return record_write();
}

void tokki_protocol_init(tokki_protocol_t *protocol, bool ready,
                         tokki_protocol_emit_fn emit, void *context)
{
    assert(ready && emit != NULL);
    memset(protocol, 0, sizeof(*protocol));
    (void) context;
}
void tokki_protocol_feed(tokki_protocol_t *protocol, const char *bytes, size_t length)
{
    (void) protocol; (void) bytes; (void) length;
    assert(false);
}
bool tokki_protocol_start_next_for_device(tokki_protocol_t *protocol, tokki_device_t requested, tokki_job_t *job)
{
    assert(protocol != NULL && locked && requested == device);
    if (!pending[requested]) return false;
    pending[requested] = false;
    strcpy_s(job->request_id, sizeof(job->request_id), "manual");
    strcpy_s(job->action_id, sizeof(job->action_id), "test.action");
    return true;
}
esp_err_t tokki_action_run(const char *id)
{
    assert(!locked && strcmp(id, "test.action") == 0 && writes == writes_before_action);
    ++actions;
    action_time = now;
    return fail_action ? ESP_FAIL : ESP_OK;
}
void tokki_protocol_finish(tokki_protocol_t *protocol, const tokki_job_t *job, esp_err_t result)
{
    assert(protocol != NULL && locked && strcmp(job->request_id, "manual") == 0);
    assert(result == (fail_action ? ESP_FAIL : ESP_OK));
    ++completions;
}
void tokki_protocol_idle_error(tokki_protocol_t *protocol, esp_err_t result)
{
    assert(protocol != NULL && locked && result == ESP_FAIL);
    ++idle_errors;
}
const char *tokki_device_name(tokki_device_t value)
{
    return value == TOKKI_DEVICE_OLED ? "oled" : "neopixel";
}

static void run_worker(tokki_device_t target, bool failed_idle, bool failed_action,
                       bool progress, TickType_t initial, unsigned warmup_waits)
{
    device = target;
    fail_idle = failed_idle;
    fail_action = failed_action;
    progress_only = progress;
    started = false;
    now = initial;
    waits = writes = actions = completions = faults = idle_errors = logs = 0;
    wait_limit = progress && device == TOKKI_DEVICE_NEOPIXEL ? 34 : 3;
    lead_in_waits = warmup_waits;
    memset(pending, 0, sizeof(pending));
    memset(colors, 0, sizeof(colors));
    if (setjmp(worker_done) == 0) {
        tasks[device].function(tasks[device].argument);
        assert(false);
    }
    assert(!locked);
}

int main(void)
{
    assert(tokki_runtime_start(true) == ESP_OK);
    assert(task_count == TOKKI_DEVICE_COUNT + 1);
    const tokki_device_t devices[] = {TOKKI_DEVICE_OLED, TOKKI_DEVICE_NEOPIXEL};
    for (unsigned i = 0; i < 2; ++i) {
        for (unsigned failure = 0; failure < 4; ++failure) {
            for (unsigned wrap = 0; wrap < 2; ++wrap) {
                TickType_t initial = wrap ? UINT32_MAX - 15 : 0;
                run_worker(devices[i], (failure & 1) != 0, (failure & 2) != 0, false, initial, 0);
                assert(actions == 1 && completions == 1 && writes == 2);
                assert(action_time - initial == 35);
                assert(write_times[1] == action_time);
                unsigned expected_faults = (failure & 1) + ((failure & 2) >> 1);
                assert(faults == expected_faults);
                assert(idle_errors == (failure & 1) && logs == faults);
                assert(pending[devices[1 - i]]);
                if (failure & 1) {
                    assert(first_wait == 5000);
                } else if (devices[i] == TOKKI_DEVICE_OLED) {
                    assert(first_wait == 60);
                } else {
                    assert(first_wait >= 6000 && first_wait <= 14000);
                }
                assert(devices[i] == TOKKI_DEVICE_OLED ? last_wait == 60 :
                       last_wait >= 6000 && last_wait <= 14000);
            }
        }
    }
    run_worker(TOKKI_DEVICE_NEOPIXEL, false, false, false, UINT32_MAX - 15, 17);
    assert(writes_before_action == 18 && colors[17] == 32);
    assert(first_wait == 60 && action_time - write_times[17] == 35);
    assert(actions == 1 && writes == 19 && colors[18] == 0);
    assert(last_wait >= 6000 && last_wait <= 14000);

    run_worker(TOKKI_DEVICE_OLED, false, false, true, UINT32_MAX - 15, 0);
    assert(writes == 4 && actions == 0);
    for (unsigned i = 1; i < writes; ++i) assert(write_times[i] - write_times[i - 1] == 60);
    run_worker(TOKKI_DEVICE_NEOPIXEL, false, false, true, UINT32_MAX - 15, 0);
    assert(writes == 35 && actions == 0 && colors[0] == 0 && colors[34] == 0);
    assert(write_times[1] - write_times[0] == first_wait);
    assert(first_wait >= 6000 && first_wait <= 14000 && last_wait >= 6000 && last_wait <= 14000);
    for (unsigned i = 1; i < 34; ++i) {
        unsigned frame = i - 1;
        assert(colors[i] == 2 * (frame <= 16 ? frame : 32 - frame));
        assert(write_times[i + 1] - write_times[i] == 60);
    }
    assert(write_times[34] - write_times[1] == 1980);
    puts("PASS: real worker idle deadlines, cross-device notifications, PC priority, reset, errors and tick wrap");
    return 0;
}
