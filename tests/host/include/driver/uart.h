#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

esp_err_t uart_driver_install(int port, int rx_size, int tx_size, int queue_size, void *queue, int flags);
esp_err_t uart_driver_delete(int port);
int uart_read_bytes(int port, void *buffer, size_t length, TickType_t timeout);
