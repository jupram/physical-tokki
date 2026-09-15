# Contributing pet gestures

Pet gestures are discoverable actions grouped by the device that performs
them. The current implementation folders are:

- `src/oled/`: eye-only expressions and animations
- `src/speaker/`: notification tones and preinstalled WAV phrases
- `src/led/`: onboard red status LED actions
- `src/neopixel/`: addressable RGB LED effects

Every action has a stable machine-readable ID, a display name, a device, and
a runner. Add its descriptor to the registry in `tokki_gestures.c`. IDs use
the form `<device>.<action>`, must be unique, and must not be renamed after a
desktop rule can reference them.

Device drivers belong in their `tokki_*` component, not in this component.
Gesture actions should compose those public APIs and leave hardware pin
assignments in `tokki_board`.

## Current actions

| ID | Device | Description |
| --- | --- | --- |
| `led.blink` | LED | Blink the onboard red LED three times |
| `neopixel.rainbow` | NeoPixel | Run one rainbow cycle and turn off |

OLED and speaker code have been moved into reusable components. Their current
self-test APIs will be converted to finite actions before they are registered.

## Checklist

1. Implement one finite action in the matching device folder.
2. Use a stable lowercase ID with the device prefix.
3. Return an `esp_err_t`; do not silently ignore a driver failure.
4. Register the descriptor in `tokki_gestures.c`.
5. Document parameters, duration, and cancellation behavior.
6. Build both the production firmware and the standalone self-test.
7. Exercise the action on hardware and note any required peripherals.