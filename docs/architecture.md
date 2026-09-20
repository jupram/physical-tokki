# Physical Tokki architecture

Physical Tokki is organized as a small monorepo with two firmware apps and a
desktop app:

```text
components/       Shared ESP-IDF board, device, and gesture components
main/             Production firmware entrypoint
self_test/        Independent manufacturing and hardware diagnostic firmware
protocol/         Versioned PC-to-pet command contract
pc-app/           Tauri desktop app with native serial transport
```

## Firmware ownership

`tokki_board` is the only component that defines Feather V2 pins and enables
the shared peripheral power rail. `tokki_led`, `tokki_neopixel`, `tokki_oled`,
and `tokki_speaker` own their device drivers. `tokki_gestures` gives finite
device actions stable IDs that can be discovered and invoked externally.

Production firmware initializes devices and exposes the action catalog. The
self-test is a separate application that diagnoses hardware through the same
public device APIs, plus raw probes such as the I2C scan where needed.

`tokki_runtime` owns USB-UART reception, protocol state, a four-slot waiting
FIFO, and one worker for each physical device. The receiver parses bounded
prefixed JSON lines, answers discovery directly, and enqueues commands under a
mutex. Each worker removes the oldest queued job for its device under that same
mutex, emits lifecycle events, and invokes the existing blocking gesture runner
outside the lock. Gestures therefore remain serial on the same driver while
OLED, speaker, status LED, and NeoPixel work can run concurrently. Reception
stays responsive and acceptance is always sent before playback starts.
Protocol responses and logs share the console's stdio serialization.

When no job for its device is queued, the OLED worker renders one idle frame
at a time and the NeoPixel worker renders intermittent teal breathing frames.
OLED choices are shuffled without immediate repeats, separated by calm holds;
NeoPixel breaths are separated by randomized dark pauses. Each worker seeds
its own small PRNG from `esp_random`, and retains no shared animation state.
The manual and idle fades share a single delay-free frame primitive.

A task notification interrupts an idle wait for new commands; I/O itself is
not cancelled. FreeRTOS timeout accounting preserves the remaining deadline
on unrelated notifications, including across tick wraparound. Completed
manual actions reset only their own device's idle state. There is no second
OLED/NeoPixel owner and no change to the gesture runners' non-cancellable
contract. Idle driver failures keep their frame state and back off five
seconds, while still allowing queued manual actions to interrupt the wait.

## Event flow

```text
PC event source -> local rule -> serial action.run -> gesture registry
                                                -> device component -> hardware
```

Event sources and local rules in this diagram are future work. The current
prototype begins at a manual desktop button. The Tauri backend opens a
user-selected serial port, negotiates `hello`, and downloads the complete
paginated action catalog. It correlates responses and lifecycle events and
reports failures rather than pretending a timed UI preview controls hardware.

Schedules, email credentials, and external integrations stay on the PC. The
pet executes actions and reports status. This keeps secrets off the ESP32 and
allows new integrations without reflashing the device.

USB serial is the first transport. Wi-Fi and BLE may carry the same logical
protocol later, but they are not part of the first milestone.