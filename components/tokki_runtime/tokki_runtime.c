#include "tokki_runtime.h"

#include <stdio.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "tokki_gestures.h"
#include "tokki_idle.h"
#include "tokki_led.h"
#include "tokki_protocol.h"

static const char *TAG = "tokki_runtime";
static tokki_protocol_t s_protocol;
static SemaphoreHandle_t s_lock;
static TaskHandle_t s_workers[TOKKI_DEVICE_COUNT];
static TaskHandle_t s_receiver;
_Static_assert(TOKKI_DEVICE_COUNT == 4, "Update the device worker tables");
static tokki_device_t s_worker_devices[TOKKI_DEVICE_COUNT] = {
    TOKKI_DEVICE_OLED,
    TOKKI_DEVICE_SPEAKER,
    TOKKI_DEVICE_LED,
    TOKKI_DEVICE_NEOPIXEL,
};

static void indicate_failure(void)
{
    esp_err_t result = tokki_led_latch_failure();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Cannot light failure LED: %s", esp_err_to_name(result));
    }
}

static void emit_frame(const char *frame, size_t length, void *context)
{
    (void) context;
    /* One stdio write shares the console's lock with ESP-IDF logs. */
    if (fwrite(frame, 1, length, stdout) != length || fflush(stdout) != 0) {
        indicate_failure();
        ESP_LOGE(TAG, "Serial response write failed");
    }
}

static void receive_commands(void *context)
{
    (void) context;
    char buffer[128];
    for (;;) {
        int received = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, buffer,
                                        sizeof(buffer), pdMS_TO_TICKS(100));
        if (received < 0) {
            indicate_failure();
            ESP_LOGE(TAG, "UART receive failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (received > 0) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            tokki_protocol_feed(&s_protocol, buffer, (size_t) received);
            bool pending = s_protocol.count > 0;
            xSemaphoreGive(s_lock);
            if (pending) {
                for (size_t index = 0; index < TOKKI_DEVICE_COUNT; ++index) {
                    xTaskNotifyGive(s_workers[index]);
                }
            }
        }
    }
}

static void execute_actions(void *context)
{
    tokki_device_t device = *(tokki_device_t *) context;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    tokki_idle_t idle;
    if (device == TOKKI_DEVICE_OLED) {
        tokki_idle_reset(&idle);
    }
    for (;;) {
        tokki_job_t job;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        bool pending = tokki_protocol_start_next_for_device(&s_protocol, device, &job);
        xSemaphoreGive(s_lock);
        if (pending) {
            esp_err_t result = tokki_action_run(job.action_id);
            if (result != ESP_OK) {
                indicate_failure();
            }
            xSemaphoreTake(s_lock, portMAX_DELAY);
            tokki_protocol_finish(&s_protocol, &job, result);
            xSemaphoreGive(s_lock);
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "%s failed: %s", job.action_id, esp_err_to_name(result));
            }
            if (device == TOKKI_DEVICE_OLED) {
                tokki_idle_reset(&idle);
            }
            continue;
        }

        if (device != TOKKI_DEVICE_OLED) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        esp_err_t result = tokki_idle_step(&idle);
        if (result != ESP_OK) {
            indicate_failure();
            xSemaphoreTake(s_lock, portMAX_DELAY);
            tokki_protocol_idle_error(&s_protocol, result);
            xSemaphoreGive(s_lock);
            ESP_LOGW(TAG, "Idle display failed: %s; retrying in 5 seconds", esp_err_to_name(result));
        }
        /* A newly queued command wakes this wait immediately, including on OLED failure. */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(
            result == ESP_OK ? TOKKI_IDLE_FRAME_MS : TOKKI_IDLE_RETRY_MS));
    }
}

esp_err_t tokki_runtime_start(bool ready)
{
    if (s_lock != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 4096, 0, 0, NULL, 0);
    if (result != ESP_OK) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return result;
    }
    tokki_protocol_init(&s_protocol, ready, emit_frame, NULL);
    static const char *const worker_names[TOKKI_DEVICE_COUNT] = {
        "tokki_oled",
        "tokki_speaker",
        "tokki_led",
        "tokki_neopixel",
    };
    for (size_t index = 0; index < TOKKI_DEVICE_COUNT; ++index) {
        if (xTaskCreate(execute_actions, worker_names[index], 8192,
                        &s_worker_devices[index], 5, &s_workers[index]) != pdPASS) {
            for (size_t created = 0; created < index; ++created) {
                vTaskDelete(s_workers[created]);
                s_workers[created] = NULL;
            }
            uart_driver_delete(CONFIG_ESP_CONSOLE_UART_NUM);
            vSemaphoreDelete(s_lock);
            s_lock = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    if (xTaskCreate(receive_commands, "tokki_serial", 8192, NULL, 6, &s_receiver) != pdPASS) {
        for (size_t index = 0; index < TOKKI_DEVICE_COUNT; ++index) {
            vTaskDelete(s_workers[index]);
            s_workers[index] = NULL;
        }
        uart_driver_delete(CONFIG_ESP_CONSOLE_UART_NUM);
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }
    for (size_t index = 0; index < TOKKI_DEVICE_COUNT; ++index) {
        xTaskNotifyGive(s_workers[index]);
    }
    return ESP_OK;
}
