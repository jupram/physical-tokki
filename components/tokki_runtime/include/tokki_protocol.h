#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "tokki_gestures.h"

#define TOKKI_FRAME_MAX 1024
#define TOKKI_REQUEST_ID_MAX 64
#define TOKKI_ACTION_ID_MAX 96
#define TOKKI_MARQUEE_TEXT_MAX 50
#define TOKKI_NOTIFICATION_LIGHT_BLUE_ACTION_ID "neopixel.notification.blue"
#define TOKKI_NOTIFICATION_LIGHT_PURPLE_ACTION_ID "neopixel.notification.purple"
#define TOKKI_NOTIFICATION_LIGHT_YELLOW_ACTION_ID "neopixel.notification.yellow"
#define TOKKI_QUEUE_CAPACITY 4
#define TOKKI_CATALOG_PAGE_SIZE 4

typedef struct {
    char request_id[TOKKI_REQUEST_ID_MAX + 1];
    char action_id[TOKKI_ACTION_ID_MAX + 1];
    char text[TOKKI_MARQUEE_TEXT_MAX + 1];
} tokki_job_t;

typedef void (*tokki_protocol_emit_fn)(const char *frame, size_t length, void *context);

typedef struct {
    char line[TOKKI_FRAME_MAX];
    size_t line_length;
    bool discarding;
    bool ready;
    tokki_job_t queue[TOKKI_QUEUE_CAPACITY];
    size_t head;
    size_t count;
    tokki_protocol_emit_fn emit;
    void *context;
} tokki_protocol_t;

/* The runtime serializes protocol state and output with one mutex. */
void tokki_protocol_init(tokki_protocol_t *protocol, bool ready,
                         tokki_protocol_emit_fn emit, void *context);
void tokki_protocol_feed(tokki_protocol_t *protocol, const char *bytes, size_t length);
bool tokki_protocol_start_next_for_device(tokki_protocol_t *protocol,
                                          tokki_device_t device,
                                          tokki_job_t *job);
void tokki_protocol_finish(tokki_protocol_t *protocol, const tokki_job_t *job,
                           esp_err_t result);
void tokki_protocol_idle_error(tokki_protocol_t *protocol, esp_err_t result);
