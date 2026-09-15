# Physical Tokki architecture

Physical Tokki is organized as a small monorepo with two firmware apps and a
desktop app:

```text
components/       Shared ESP-IDF board, device, and gesture components
main/             Production firmware entrypoint
self_test/        Independent manufacturing and hardware diagnostic firmware
protocol/         Versioned PC-to-pet command contract
pc-app/           Tauri desktop app (scaffolded separately)
```

## Firmware ownership

`tokki_board` is the only component that defines Feather V2 pins and enables
the shared peripheral power rail. `tokki_led`, `tokki_neopixel`, `tokki_oled`,
and `tokki_speaker` own their device drivers. `tokki_gestures` gives finite
device actions stable IDs that can be discovered and invoked externally.

Production firmware initializes devices and exposes the action catalog. The
self-test is a separate application that diagnoses hardware through the same
public device APIs, plus raw probes such as the I2C scan where needed.

## Event flow

```text
PC event source -> local rule -> serial action.run -> gesture registry
                                                -> device component -> hardware
```

Schedules, email credentials, and external integrations stay on the PC. The
pet executes actions and reports status. This keeps secrets off the ESP32 and
allows new integrations without reflashing the device.

USB serial is the first transport. Wi-Fi and BLE may carry the same logical
protocol later, but they are not part of the first milestone.