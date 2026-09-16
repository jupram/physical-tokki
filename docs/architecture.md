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

`tokki_runtime` owns USB-UART reception, the protocol state, a four-slot FIFO,
and one hardware worker. The receiver parses bounded prefixed JSON lines,
answers discovery directly, and enqueues commands under a mutex. The worker
removes jobs under that same mutex, emits lifecycle events, and invokes the
existing blocking gesture runners outside the lock. This keeps reception
responsive while guaranteeing acceptance is sent before playback starts.
Protocol responses and logs share the console's stdio serialization.

When no job is queued, that worker renders one idle OLED frame at a time from
the existing pet-eye renderer. A task notification interrupts the inter-frame
wait for new commands; I/O itself is not cancelled. There is no second OLED
owner and no change to the gesture runners' non-cancellable contract.

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