#include "tokki_protocol.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "tokki_gestures.h"

#define PREFIX "TOKKI/1 "
#define PREFIX_LENGTH (sizeof(PREFIX) - 1)
#define MARQUEE_ACTION_ID "oled.marquee"

static bool add_string(cJSON *object, const char *key, const char *value)
{
    return cJSON_AddStringToObject(object, key, value) != NULL;
}

static void emit_json(tokki_protocol_t *protocol, cJSON *object, const char *id)
{
    char frame[TOKKI_FRAME_MAX + 1];
    memcpy(frame, PREFIX, PREFIX_LENGTH);
    /* cJSON needs spare bytes beyond its serialized text. Reserve LF as well. */
    bool printed = object != NULL && cJSON_PrintPreallocated(
        object, frame + PREFIX_LENGTH, TOKKI_FRAME_MAX - (int) PREFIX_LENGTH - 1, false);
    if (!printed) {
        int written = snprintf(frame, sizeof(frame),
            PREFIX "{\"id\":%s%s%s,\"ok\":false,\"error\":{"
            "\"code\":\"internal_error\",\"message\":\"Response serialization failed\"}}\n",
            id != NULL ? "\"" : "", id != NULL ? id : "null", id != NULL ? "\"" : "");
        protocol->emit(frame, (size_t) written, protocol->context);
    } else {
        size_t length = strlen(frame);
        frame[length++] = '\n';
        protocol->emit(frame, length, protocol->context);
    }
    cJSON_Delete(object);
}

static cJSON *response(const char *id, bool ok)
{
    cJSON *object = cJSON_CreateObject();
    if (object == NULL) {
        return NULL;
    }
    bool valid = id != NULL ? add_string(object, "id", id) :
                             cJSON_AddNullToObject(object, "id") != NULL;
    if (!valid || cJSON_AddBoolToObject(object, "ok", ok) == NULL) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static void error_response(tokki_protocol_t *protocol, const char *id,
                            const char *code, const char *message)
{
    cJSON *object = response(id, false);
    cJSON *error = cJSON_AddObjectToObject(object, "error");
    if (error == NULL || !add_string(error, "code", code) ||
        !add_string(error, "message", message)) {
        cJSON_Delete(object);
        object = NULL;
    }
    emit_json(protocol, object, id);
}

static bool valid_identifier(const cJSON *value, size_t maximum)
{
    if (!cJSON_IsString(value) || value->valuestring[0] == '\0' ||
        strlen(value->valuestring) > maximum) {
        return false;
    }
    for (const char *character = value->valuestring; *character != '\0'; ++character) {
        if (!((*character >= 'a' && *character <= 'z') ||
              (*character >= 'A' && *character <= 'Z') ||
              (*character >= '0' && *character <= '9') ||
              strchr("_.:-", *character) != NULL)) {
            return false;
        }
    }
    return true;
}

static void hello(tokki_protocol_t *protocol, const char *id)
{
    cJSON *object = response(id, true);
    cJSON *result = cJSON_AddObjectToObject(object, "result");
    if (result == NULL || cJSON_AddNumberToObject(result, "protocol", 1) == NULL ||
        !add_string(result, "firmware", "0.2.0") ||
        !add_string(result, "board", "adafruit_feather_esp32_v2") ||
        cJSON_AddBoolToObject(result, "ready", protocol->ready) == NULL ||
        cJSON_AddNumberToObject(result, "queueCapacity", TOKKI_QUEUE_CAPACITY) == NULL) {
        cJSON_Delete(object);
        object = NULL;
    }
    emit_json(protocol, object, id);
}

static void list_actions(tokki_protocol_t *protocol, const char *id, const cJSON *params)
{
    const cJSON *cursor_value = cJSON_GetObjectItemCaseSensitive(params, "cursor");
    size_t total = tokki_action_count();
    double cursor_number = cursor_value == NULL ? 0 : cursor_value->valuedouble;
    if (cursor_value != NULL &&
        (!cJSON_IsNumber(cursor_value) || !isfinite(cursor_number) ||
         cursor_number < 0 || cursor_number > (double) total ||
         floor(cursor_number) != cursor_number)) {
        error_response(protocol, id, "invalid_params", "cursor must be an integer from 0 to total");
        return;
    }
    size_t cursor = (size_t) cursor_number;
    size_t end = cursor + TOKKI_CATALOG_PAGE_SIZE;
    if (end > total) {
        end = total;
    }
    cJSON *object = response(id, true);
    cJSON *result = cJSON_AddObjectToObject(object, "result");
    cJSON *actions = cJSON_AddArrayToObject(result, "actions");
    bool valid = actions != NULL;
    for (size_t index = cursor; valid && index < end; ++index) {
        const tokki_action_descriptor_t *action = tokki_action_at(index);
        cJSON *descriptor = cJSON_CreateObject();
        if (descriptor == NULL || !cJSON_AddItemToArray(actions, descriptor)) {
            cJSON_Delete(descriptor);
            valid = false;
            break;
        }
        valid = add_string(descriptor, "id", action->id) &&
                add_string(descriptor, "name", action->display_name) &&
                add_string(descriptor, "device", tokki_device_name(action->device)) &&
                cJSON_AddBoolToObject(descriptor, "cancellable", action->cancellable) != NULL;
    }
    if (!valid || cJSON_AddNumberToObject(result, "total", (double) total) == NULL ||
        (end < total ? cJSON_AddNumberToObject(result, "nextCursor", (double) end) :
                       cJSON_AddNullToObject(result, "nextCursor")) == NULL) {
        cJSON_Delete(object);
        object = NULL;
    }
    emit_json(protocol, object, id);
}

static bool valid_marquee_text(const cJSON *value)
{
    if (!cJSON_IsString(value)) {
        return false;
    }
    size_t length = strlen(value->valuestring);
    if (length == 0 || length > TOKKI_MARQUEE_TEXT_MAX) {
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        unsigned char character = (unsigned char) value->valuestring[index];
        if (character < 0x20 || character > 0x7E) {
            return false;
        }
    }
    return true;
}

static void run_action(tokki_protocol_t *protocol, const char *id, const cJSON *params)
{
    const cJSON *action_id = cJSON_GetObjectItemCaseSensitive(params, "actionId");
    if (!valid_identifier(action_id, TOKKI_ACTION_ID_MAX)) {
        error_response(protocol, id, "invalid_params", "Provide a stable actionId");
        return;
    }
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(params, "text");
    bool scrolling_text = strcmp(action_id->valuestring, TOKKI_OLED_SCROLLING_TEXT_ACTION_ID) == 0;
    if (cJSON_GetArraySize(params) != (text != NULL ? 2 : 1) ||
        (text != NULL && (!scrolling_text || !valid_marquee_text(text)))) {
        error_response(protocol, id, "invalid_params",
                       "Only oled.scrolling_text accepts optional text: 1 to 50 printable ASCII characters");
        return;
    }
    if (tokki_action_find(action_id->valuestring) == NULL) {
        error_response(protocol, id, "action_not_found", "Unknown action");
        return;
    }
    if (!protocol->ready) {
        error_response(protocol, id, "internal_error", "Board initialization failed; see device logs");
        return;
    }
    if (protocol->count == TOKKI_QUEUE_CAPACITY) {
        error_response(protocol, id, "device_busy", "Action queue is full");
        return;
    }
    cJSON *object = response(id, true);
    cJSON *result = cJSON_AddObjectToObject(object, "result");
    if (result == NULL || cJSON_AddBoolToObject(result, "accepted", true) == NULL) {
        cJSON_Delete(object);
        error_response(protocol, id, "internal_error", "Cannot allocate acceptance response");
        return;
    }
    tokki_job_t *job = &protocol->queue[(protocol->head + protocol->count) % TOKKI_QUEUE_CAPACITY];
    memcpy(job->request_id, id, strlen(id) + 1);
    memcpy(job->action_id, action_id->valuestring, strlen(action_id->valuestring) + 1);
    const char *queued_text = text != NULL ? text->valuestring :
                              scrolling_text ? TOKKI_OLED_SCROLLING_TEXT_DEFAULT : "";
    memcpy(job->text, queued_text, strlen(queued_text) + 1);
    ++protocol->count;
    emit_json(protocol, object, id);
}

static void run_marquee(tokki_protocol_t *protocol, const char *id, const cJSON *params)
{
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(params, "text");
    if (!valid_marquee_text(text) || cJSON_GetArraySize(params) != 1) {
        error_response(protocol, id, "invalid_params",
                       "text must be 1 to 50 printable ASCII characters");
        return;
    }
    if (!protocol->ready) {
        error_response(protocol, id, "internal_error", "Board initialization failed; see device logs");
        return;
    }
    if (protocol->count == TOKKI_QUEUE_CAPACITY) {
        error_response(protocol, id, "device_busy", "Action queue is full");
        return;
    }
    cJSON *object = response(id, true);
    cJSON *result = cJSON_AddObjectToObject(object, "result");
    if (result == NULL || cJSON_AddBoolToObject(result, "accepted", true) == NULL) {
        cJSON_Delete(object);
        error_response(protocol, id, "internal_error", "Cannot allocate acceptance response");
        return;
    }
    tokki_job_t *job = &protocol->queue[(protocol->head + protocol->count) % TOKKI_QUEUE_CAPACITY];
    memcpy(job->request_id, id, strlen(id) + 1);
    memcpy(job->action_id, MARQUEE_ACTION_ID, sizeof(MARQUEE_ACTION_ID));
    memcpy(job->text, text->valuestring, strlen(text->valuestring) + 1);
    ++protocol->count;
    emit_json(protocol, object, id);
}

static const char *notification_light_action(const cJSON *color)
{
    if (!cJSON_IsString(color)) {
        return NULL;
    }
    if (strcmp(color->valuestring, "blue") == 0) {
        return TOKKI_NOTIFICATION_LIGHT_BLUE_ACTION_ID;
    }
    if (strcmp(color->valuestring, "purple") == 0) {
        return TOKKI_NOTIFICATION_LIGHT_PURPLE_ACTION_ID;
    }
    if (strcmp(color->valuestring, "yellow") == 0) {
        return TOKKI_NOTIFICATION_LIGHT_YELLOW_ACTION_ID;
    }
    return NULL;
}

static void run_notification_light(tokki_protocol_t *protocol, const char *id,
                                   const cJSON *params)
{
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(params, "text");
    const char *action_id = notification_light_action(
        cJSON_GetObjectItemCaseSensitive(params, "color"));
    if (!valid_marquee_text(text) || action_id == NULL || cJSON_GetArraySize(params) != 2) {
        error_response(protocol, id, "invalid_params",
                       "text must be 1 to 50 printable ASCII characters and color must be blue, purple or yellow");
        return;
    }
    if (!protocol->ready) {
        error_response(protocol, id, "internal_error", "Board initialization failed; see device logs");
        return;
    }
    if (protocol->count == TOKKI_QUEUE_CAPACITY) {
        error_response(protocol, id, "device_busy", "Action queue is full");
        return;
    }
    cJSON *object = response(id, true);
    cJSON *result = cJSON_AddObjectToObject(object, "result");
    if (result == NULL || cJSON_AddBoolToObject(result, "accepted", true) == NULL) {
        cJSON_Delete(object);
        error_response(protocol, id, "internal_error", "Cannot allocate acceptance response");
        return;
    }
    tokki_job_t *job = &protocol->queue[(protocol->head + protocol->count) % TOKKI_QUEUE_CAPACITY];
    memcpy(job->request_id, id, strlen(id) + 1);
    memcpy(job->action_id, action_id, strlen(action_id) + 1);
    memcpy(job->text, text->valuestring, strlen(text->valuestring) + 1);
    ++protocol->count;
    emit_json(protocol, object, id);
}

static bool bounded_json(const char *json)
{
    unsigned depth = 0;
    bool in_string = false;
    for (const char *character = json; *character != '\0'; ++character) {
        if (in_string) {
            if (*character == '\\') {
                if (character[1] == '\0' || strncmp(character, "\\u0000", 6) == 0) {
                    return false;
                }
                ++character;
            } else if (*character == '"') {
                in_string = false;
            }
        } else if (*character == '"') {
            in_string = true;
        } else if (*character == '{' || *character == '[') {
            if (++depth > 8) {
                return false;
            }
        } else if (*character == '}' || *character == ']') {
            if (depth == 0) {
                return false;
            }
            --depth;
        }
    }
    return depth == 0 && !in_string;
}

static void process_line(tokki_protocol_t *protocol)
{
    if (protocol->line_length < PREFIX_LENGTH ||
        memcmp(protocol->line, PREFIX, PREFIX_LENGTH) != 0) {
        return;
    }
    const char *json = protocol->line + PREFIX_LENGTH;
    if (!bounded_json(json)) {
        error_response(protocol, NULL, "invalid_request", "Invalid JSON, NUL escape or nesting beyond 8 levels");
        return;
    }
    cJSON *request = cJSON_ParseWithLengthOpts(
        json, protocol->line_length - PREFIX_LENGTH + 1, NULL, true);
    const cJSON *id_value = cJSON_GetObjectItemCaseSensitive(request, "id");
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(request, "method");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(request, "params");
    const char *id = valid_identifier(id_value, TOKKI_REQUEST_ID_MAX) ? id_value->valuestring : NULL;
    if (!cJSON_IsObject(request) || id == NULL ||
        !cJSON_IsString(method) || !cJSON_IsObject(params)) {
        error_response(protocol, id, "invalid_request", "Expected id, method and object params");
    } else if (strcmp(method->valuestring, "hello") == 0) {
        hello(protocol, id);
    } else if (strcmp(method->valuestring, "actions.list") == 0) {
        list_actions(protocol, id, params);
    } else if (strcmp(method->valuestring, "action.run") == 0) {
        run_action(protocol, id, params);
    } else if (strcmp(method->valuestring, "oled.marquee") == 0) {
        run_marquee(protocol, id, params);
    } else if (strcmp(method->valuestring, "neopixel.notification") == 0) {
        run_notification_light(protocol, id, params);
    } else if (strcmp(method->valuestring, "action.stop") == 0) {
        const cJSON *run_id = cJSON_GetObjectItemCaseSensitive(params, "requestId");
        if (!valid_identifier(run_id, TOKKI_REQUEST_ID_MAX) || cJSON_GetArraySize(params) != 1) {
            error_response(protocol, id, "invalid_params", "Provide the original requestId");
        } else {
            error_response(protocol, id, "not_cancellable", "Prototype gestures cannot be cancelled");
        }
    } else {
        error_response(protocol, id, "method_not_found", "Unsupported method");
    }
    cJSON_Delete(request);
}

void tokki_protocol_init(tokki_protocol_t *protocol, bool ready,
                         tokki_protocol_emit_fn emit, void *context)
{
    memset(protocol, 0, sizeof(*protocol));
    protocol->ready = ready;
    protocol->emit = emit;
    protocol->context = context;
}

void tokki_protocol_feed(tokki_protocol_t *protocol, const char *bytes, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        char character = bytes[index];
        if (character == '\n') {
            if (!protocol->discarding) {
                protocol->line[protocol->line_length] = '\0';
                process_line(protocol);
            }
            protocol->line_length = 0;
            protocol->discarding = false;
        } else if (!protocol->discarding) {
            if (character == '\0' || protocol->line_length == TOKKI_FRAME_MAX - 1) {
                bool framed = protocol->line_length >= PREFIX_LENGTH &&
                              memcmp(protocol->line, PREFIX, PREFIX_LENGTH) == 0;
                protocol->discarding = true;
                if (framed) {
                    error_response(protocol, NULL, "invalid_request", "Frame too large or contains NUL");
                }
            } else {
                protocol->line[protocol->line_length++] = character;
            }
        }
    }
}

static void lifecycle(tokki_protocol_t *protocol, const char *event,
                       const tokki_job_t *job, esp_err_t result)
{
    cJSON *object = cJSON_CreateObject();
    cJSON *data = cJSON_AddObjectToObject(object, "data");
    bool valid = data != NULL && add_string(object, "event", event);
    if (valid && job != NULL) {
        valid = add_string(data, "requestId", job->request_id) &&
                add_string(data, "actionId", job->action_id);
    }
    if (valid && result != ESP_OK) {
        valid = add_string(data, "code", "driver_error") &&
                add_string(data, "message", esp_err_to_name(result));
    }
    if (!valid) {
        cJSON_Delete(object);
        object = NULL;
    }
    emit_json(protocol, object, job != NULL ? job->request_id : NULL);
}

bool tokki_protocol_start_next_for_device(tokki_protocol_t *protocol,
                                          tokki_device_t device,
                                          tokki_job_t *job)
{
    size_t offset = 0;
    for (; offset < protocol->count; ++offset) {
        size_t index = (protocol->head + offset) % TOKKI_QUEUE_CAPACITY;
        const tokki_action_descriptor_t *action =
            tokki_action_find(protocol->queue[index].action_id);
        bool marquee = strcmp(protocol->queue[index].action_id, MARQUEE_ACTION_ID) == 0;
        bool notification_light =
            strcmp(protocol->queue[index].action_id, TOKKI_NOTIFICATION_LIGHT_BLUE_ACTION_ID) == 0 ||
            strcmp(protocol->queue[index].action_id, TOKKI_NOTIFICATION_LIGHT_PURPLE_ACTION_ID) == 0 ||
            strcmp(protocol->queue[index].action_id, TOKKI_NOTIFICATION_LIGHT_YELLOW_ACTION_ID) == 0;
        if ((marquee && device == TOKKI_DEVICE_OLED) ||
            (notification_light && device == TOKKI_DEVICE_NEOPIXEL) ||
            (action != NULL && action->device == device)) {
            *job = protocol->queue[index];
            break;
        }
    }
    if (offset == protocol->count) {
        return false;
    }
    for (size_t current = offset; current + 1 < protocol->count; ++current) {
        size_t destination = (protocol->head + current) % TOKKI_QUEUE_CAPACITY;
        size_t source = (protocol->head + current + 1) % TOKKI_QUEUE_CAPACITY;
        protocol->queue[destination] = protocol->queue[source];
    }
    --protocol->count;
    lifecycle(protocol, "action.started", job, ESP_OK);
    return true;
}

void tokki_protocol_finish(tokki_protocol_t *protocol, const tokki_job_t *job,
                           esp_err_t result)
{
    lifecycle(protocol, result == ESP_OK ? "action.completed" : "action.failed", job, result);
}

void tokki_protocol_idle_error(tokki_protocol_t *protocol, esp_err_t result)
{
    lifecycle(protocol, "idle.error", NULL, result);
}
