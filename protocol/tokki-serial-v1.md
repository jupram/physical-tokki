# Tokki Serial Protocol v1

Tokki v1 uses UTF-8, newline-delimited JSON over the board's USB serial port.
Every protocol frame starts with `TOKKI/1 ` so clients can ignore ordinary
ESP-IDF log output on the same port. A frame must fit on one line and must not
exceed 1024 bytes.

## Request envelope

```text
TOKKI/1 {"id":"1","method":"hello","params":{}}
```

- `id` is a non-empty client correlation string.
- `method` is one of the methods below.
- `params` is an object and may be empty.

## Response envelope

```text
TOKKI/1 {"id":"1","ok":true,"result":{"protocol":1,"firmware":"0.1.0"}}
TOKKI/1 {"id":"2","ok":false,"error":{"code":"action_not_found","message":"Unknown action"}}
```

A request receives exactly one response with the same `id`. Long-running
actions additionally emit lifecycle events.

## Methods

### `hello`

Returns protocol version, firmware version, board identifier, and readiness.

### `actions.list`

Returns descriptors generated from the firmware gesture registry:

```text
TOKKI/1 {"id":"2","ok":true,"result":{"actions":[{"id":"led.blink","name":"Blink status LED","device":"led","cancellable":false}]}}
```

### `action.run`

Queues one action. Milestone-one actions take no parameters.

```text
TOKKI/1 {"id":"3","method":"action.run","params":{"actionId":"neopixel.rainbow"}}
```

The response confirms acceptance. Execution state is reported separately:

```text
TOKKI/1 {"event":"action.started","data":{"requestId":"3","actionId":"neopixel.rainbow"}}
TOKKI/1 {"event":"action.completed","data":{"requestId":"3","actionId":"neopixel.rainbow"}}
```

### `action.stop`

Requests cancellation using the original run request ID. Non-cancellable or
already completed actions return a structured error.

## Error codes

- `invalid_request`: malformed JSON or missing envelope fields
- `method_not_found`: unsupported method
- `action_not_found`: unknown stable action ID
- `invalid_params`: unsupported or invalid action parameters
- `device_busy`: the owning device cannot accept another action
- `not_cancellable`: the selected action cannot be stopped
- `internal_error`: unexpected firmware or driver failure

Clients must ignore lines without the `TOKKI/1 ` prefix, unknown event names,
and additional object properties. Firmware must reject oversized input and
continue processing subsequent complete lines.