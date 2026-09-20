#pragma once

#include "freertos/FreeRTOS.h"

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
typedef struct {
    TickType_t started;
} TimeOut_t;

void vTaskDelay(TickType_t ticks);
BaseType_t xTaskCreate(TaskFunction_t function, const char *name, uint32_t stack_depth,
                       void *argument, UBaseType_t priority, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t handle);
void xTaskNotifyGive(TaskHandle_t handle);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t timeout);
void vTaskSetTimeOutState(TimeOut_t *timeout);
BaseType_t xTaskCheckForTimeOut(TimeOut_t *timeout, TickType_t *remaining);