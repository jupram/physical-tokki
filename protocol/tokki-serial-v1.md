# Tokki Serial Protocol v1

Tokki v1 uses UTF-8, newline-delimited JSON over the board's USB serial port.
Every protocol frame starts with `TOKKI/1 ` so clients can ignore ordinary
ESP-IDF log output on the same port. A frame must fit on one line and must not
exceed 1024 bytes, including the prefix and newline. LF and CRLF are accepted.
The Feather V2 uses its USB-to-UART bridge at **115200 baud, 8 data bits, no
parity, 1 stop bit, no flow control**. Only one PC process may own the port;
close the ESP-IDF serial monitor before connecting the desktop app.

The production firmware implements this contract. The desktop connects to a
user-selected serial port, performs `hello`, then follows every `actions.list`
page. It does not maintain a separate hardcoded gesture catalog.

## Request envelope

```text
TOKKI/1 {"id":"1","method":"hello","params":{}}
```

- `id` is a non-empty client correlation string of at most 64 ASCII characters,
  using `A-Z`, `a-z`, `0-9`, `_`, `.`, `:`, or `-`. Use a new ID for every request.
- `method` is one of the methods below.
- `params` is an object and may be empty.
- Embedded NULs, NUL escapes, and nesting deeper than eight JSON containers are
  rejected. Unknown envelope properties are ignored.

## Response envelope

```text
TOKKI/1 {"id":"1","ok":true,"result":{"protocol":1,"firmware":"0.2.0","board":"adafruit_feather_esp32_v2","ready":true,"queueCapacity":4}}
TOKKI/1 {"id":"2","ok":false,"error":{"code":"action_not_found","message":"Unknown action"}}
```

A request receives exactly one response with the same `id`. Long-running
actions additionally emit lifecycle events. An invalid envelope or oversized
frame whose ID cannot be safely recovered receives an error with `id:null`.

## Methods

### `hello`

Returns protocol version, firmware version, board identifier, readiness, and
queue capacity. `ready` reflects successful board, status LED, and NeoPixel
initialization. OLED and speaker drivers initialize lazily: a registered action
does **not** prove that its peripheral is attached or functioning. Driver
failures are reported when that action runs.

### `actions.list`

Returns descriptors generated from the firmware gesture registry in pages of
at most four, keeping every response below the frame limit:

```text
TOKKI/1 {"id":"2","method":"actions.list","params":{"cursor":0}}
```

The result has this shape:

```json
{
  "actions": [
    {"id": "led.blink", "name": "Blink status LED", "device": "led", "cancellable": false}
  ],
  "total": 31,
  "nextCursor": 4
}
```

The example above omits the other three descriptors for readability. `cursor`
defaults to zero and must be an integer from zero through `total`. Clients must
request `nextCursor` until it is `null`; do not assume the catalog has 31 entries
or that every page is full. Requesting `cursor == total` returns an empty page
and `nextCursor:null`. Descriptors have no duration field in v1.

### `action.run`

Queues one action. Prototype actions take no parameters; `params` must contain
only `actionId` (a stable identifier of at most 96 characters).

```text
TOKKI/1 {"id":"3","method":"action.run","params":{"actionId":"neopixel.rainbow"}}
```

The response `{"accepted":true}` in `result` confirms acceptance, not playback
completion. There is one worker per physical device (OLED, speaker, status LED,
and NeoPixel), with up to **four waiting actions globally plus one running
action per device**. Actions for different devices can run concurrently.
Actions for the same device retain FIFO order. Full queues return `device_busy`
without accepting or running that request. Firmware emits acceptance before
that request's `action.started` event.

Execution state is reported separately:

```text
TOKKI/1 {"event":"action.started","data":{"requestId":"3","actionId":"neopixel.rainbow"}}
TOKKI/1 {"event":"action.completed","data":{"requestId":"3","actionId":"neopixel.rainbow"}}
TOKKI/1 {"event":"action.failed","data":{"requestId":"4","actionId":"oled.blink","code":"driver_error","message":"ESP_ERR_TIMEOUT"}}
```

Each started action ends in either `action.completed` or `action.failed`, except
if the device resets or transport is lost. A failed action does not discard
later queued actions. Reception, `hello`, and discovery continue during
playback. Do not automatically retry `action.run` after a response timeout:
delivery may have succeeded and retrying could play the gesture twice.

### `oled.marquee`

Queues one parameterized OLED marquee without adding dynamic content to the
fixed gesture catalog. `params` must contain only `text`, with 1 to 40 printable
ASCII characters (`0x20` through `0x7E`).

```text
TOKKI/1 {"id":"4","method":"oled.marquee","params":{"text":"Teams: Build 42!"}}
```

The response and `action.started`/`action.completed`/`action.failed` lifecycle
contract is the same as `action.run`, with `actionId:"oled.marquee"`. The OLED
worker scrolls the text from right to left once, then autonomous idle resumes.
Marquees use the OLED device's FIFO ordering and the shared four-slot waiting
queue. Clients must not retry after an ambiguous timeout.

### `neopixel.notification`

Queues a parameterized NeoPixel color for the duration of the corresponding
OLED marquee without adding dynamic notification entries to the fixed action
catalog. `params` must contain only the same validated `text` used for
`oled.marquee` and a `color` of `blue`, `purple`, or `yellow`.

```text
TOKKI/1 {"id":"5","method":"neopixel.notification","params":{"text":"Teams: Build 42!","color":"purple"}}
```

The lifecycle contract matches `action.run`. Its action ID is
`neopixel.notification.<color>`. The NeoPixel worker turns the selected color
on, waits for the exact number of 45 ms frames used by one OLED marquee scroll,
then turns the NeoPixel off. Send this request with the matching `oled.marquee`
and speaker `action.run`; separate device workers execute them concurrently.
It uses the NeoPixel device's FIFO ordering and shared waiting queue.

### `action.stop`

Accepts `params:{"requestId":"3"}`. All prototype gestures are non-cancellable,
including queued ones, so valid stop requests return `not_cancellable`.
Disconnecting the PC app does not cancel already accepted gestures.

## Autonomous idle behavior

When the action queue is empty, the same hardware worker draws a repeating
sequence of happy/blinking eyes, left/right glances, and curious eyes. It uses
the existing renderer at a nominal 60 ms per frame, not blocking gesture calls.
A queued command wakes the inter-frame wait and runs after the current OLED
write finishes. No concurrent idle and commanded writes occur on any driver.
Physical I/O latency or an OLED initialization timeout can delay handoff; this
is not a hard real-time latency guarantee.

After the queue drains, the idle sequence restarts with happy eyes. This
intentionally replaces the last commanded OLED frame after its gesture's
finite run finishes (including the three-second drink-water message).
Idle runs even when no PC is connected. `hello` and discovery do not pause it.

A failed idle display write emits
`{"event":"idle.error","data":{"code":"driver_error","message":"..."}}` and
retries after five seconds to avoid a tight error loop. Incoming PC gestures
still wake that wait immediately, so a missing OLED does not prevent LED,
NeoPixel, or speaker commands.

Startup and runtime hardware failures also latch the onboard red status LED on
until reset. Later successful operations do not clear it; `led.blink` cannot
override it. This is historical fault indication, not a change to the startup
`hello.ready` field. Invalid requests and PC-side port-open failures do not
set this firmware latch. Illumination requires a working LED/GPIO driver.

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

## Verification

Run `.\tests\host\run.ps1 -Runtime` after an ESP-IDF production build has
restored the managed cJSON dependency. It compiles the real protocol, catalog,
and idle renderer with mocked hardware, checking fragmented/combined frames,
CRLF, the exact 1024-byte boundary, malformed input recovery, all catalog pages,
per-device FIFO capacity/order, cross-device dispatch, acceptance/lifecycle
ordering, errors, and idle frames.
It does not emulate UART electrical behavior or physical OLED timing.

For cross-language conformance tests, add `-WireFixturePath <output-file>` to
the host command. This exports actual C-generated hello, catalog, acceptance,
lifecycle, idle-error, and unknown-action frames without physical hardware.