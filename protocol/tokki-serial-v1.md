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
  "total": 48,
  "nextCursor": 4
}
```

The example above omits the other three descriptors for readability. `cursor`
defaults to zero and must be an integer from zero through `total`. Clients must
request `nextCursor` until it is `null`; do not assume the catalog has 48 entries
or that every page is full. Requesting `cursor == total` returns an empty page
and `nextCursor:null`. Descriptors have no duration field in v1.

### `action.run`

Queues one action. `params` must contain `actionId` (a stable identifier of at
most 96 characters). All actions except `oled.scrolling_text` accept no other
parameters.

```text
TOKKI/1 {"id":"3","method":"action.run","params":{"actionId":"neopixel.rainbow"}}
```

The catalog contains **48 actions, including 24 OLED actions**. The appended
`oled.scrolling_text` descriptor has name `Scrolling text`, device `oled`, and
`cancellable:false`; existing action IDs and registry positions are unchanged.
It accepts an optional `text` string of **1-50 printable ASCII characters**
(`0x20` through `0x7E`). Omitting `text` uses exactly `Hello from Tokki!`.
Empty strings, non-string values, controls, non-ASCII characters, text longer
than 50 characters, duplicate/extra properties, or `text` on any other action
are rejected as `invalid_params`; malformed JSON and NUL escapes remain
`invalid_request`. Firmware never truncates supplied text.

```text
TOKKI/1 {"id":"title","method":"action.run","params":{"actionId":"oled.scrolling_text","text":"Teams: Build 42!"}}
TOKKI/1 {"id":"greeting","method":"action.run","params":{"actionId":"oled.scrolling_text"}}
```

Accepted jobs own a copy of their text until execution. Scrolling uses the
same renderer and runner as legacy `oled.marquee`: one right-to-left pass,
two pixels per 45 ms frame, with no extra eye restoration frame. Its nominal
duration is `(63 + 6 * character_count) * 45 ms` (7.425 seconds by default;
16.335 seconds for 50 characters), excluding hardware/scheduler overhead.
The default greeting's last frame is blank; normal OLED idle resumes after
the queue drains. Lifecycle events use `actionId:"oled.scrolling_text"` for
both custom and default text, and the first driver failure stops playback.

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

Legacy compatibility method; new clients should use `action.run` with
`actionId:"oled.scrolling_text"`. It queues one parameterized OLED marquee
without adding a separate catalog entry. `params` must contain only `text`, with 1 to 50 printable
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

Legacy compatibility method, no longer used by the fixed notification relay.
Queues a parameterized NeoPixel color for the duration of the corresponding
OLED marquee without adding dynamic notification entries to the fixed action
catalog. `params` must contain only the same validated 1-50-character `text` used for
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

### Fixed desktop notification mapping

The desktop relay uses existing catalog gestures plus the new scrolling-text
action, not the two legacy methods:

| Notification | OLED eyes first | Speaker | NeoPixel | OLED title second |
| --- | --- | --- | --- | --- |
| Teams | `oled.curious` | `speaker.trill` | `neopixel.rainbow` | `oled.scrolling_text` |
| Outlook mail | `oled.happy` | `speaker.chime` | `neopixel.pulse_blue` | `oled.scrolling_text` |
| Outlook meeting/reminder | `oled.surprised` | `speaker.whistle` | `neopixel.blink_yellow` | `oled.scrolling_text` |

Send the eye action before the title action; the OLED worker's FIFO ensures
eyes complete before title scrolling starts. Speaker and NeoPixel actions can
overlap the OLED sequence on their own workers and retain their existing
finite durations and final states; lights are not extended to the title's
duration. The title is passed in `action.run.params.text`. No notification
bundle, new sound, or new light action is registered. Autonomous idle behavior
is unchanged.

### `action.stop`

Accepts `params:{"requestId":"3"}`. All prototype gestures are non-cancellable,
including queued ones, so valid stop requests return `not_cancellable`.
Disconnecting the PC app does not cancel already accepted gestures.

## Autonomous idle behavior

When a device has no queued or executing action, its existing worker may
render idle frames. OLED and NeoPixel idle independently, so their animations
overlap naturally; speaker and status LED have no autonomous animations.
No extra idle task or blocking gesture runner is used.

The OLED holds centered open eyes for 0.6-1.2 seconds between animations. It
shuffles eight choices: blink, look left, look right, look up, happy, curious,
lovey-dovey, and shy. Each is selected once per shuffled round, with no
immediate repeat across rounds. Left and right glances each append a
nine-frame crescent blink after the pupils return to center: look -> center
-> blink -> calm hold. These two extra blinks do not consume shuffled choices.
Animation frames retain their nominal 60 ms timing, so each added blink lasts
0.54 seconds. The last frame re-centers the eyes before the next calm hold.

The NeoPixel stays dark for a randomly chosen 6-14 seconds, then plays the
existing teal breathing fade: 33 frames at 60 ms (1.98 seconds), with green
and blue rising from 0 to 32 and back to 0; red remains off. It then starts
another dark pause. Each device has separately seeded idle random state.

A queued command wakes its device's idle wait immediately and runs after the
current hardware write finishes. Unrelated notifications do not shorten
frames, pauses, or error retry delays. No concurrent idle and commanded writes
occur on any driver. Physical I/O latency or an OLED initialization timeout
can delay handoff; this is not a hard real-time latency guarantee.

After that device's queue drains, OLED restarts with calm open eyes and
NeoPixel restarts with a dark pause. This intentionally replaces the last
commanded frame after its finite run finishes (including the three-second
drink-water message). Other devices continue independently. Idle runs even
without a PC connection; `hello` and discovery do not pause it.

A failed idle OLED or NeoPixel write emits
`{"event":"idle.error","data":{"code":"driver_error","message":"..."}}` and
retries after five seconds to avoid a tight error loop; firmware logs identify
the failing device. The animation state advances only on successful writes.
Incoming PC gestures still wake that wait immediately, so a failing idle
device does not prevent commands on other devices.

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
ordering, errors, and idle frames. Scrolling tests cover the default catalog
action, all 50 characters (51 rejected), printable-ASCII validation, owned
queued text, eyes-before-title FIFO execution, frame bounds and original
45 ms/two-pixel timing, driver errors, idle resumption, and both legacy methods.
It does not emulate UART electrical behavior or physical OLED timing.

For cross-language conformance tests, add `-WireFixturePath <output-file>` to
the host command. This exports actual C-generated hello, catalog, acceptance,
lifecycle, idle-error, and unknown-action frames without physical hardware.