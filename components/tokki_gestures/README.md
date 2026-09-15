# Contributing pet gestures

Pet gestures are discoverable actions grouped by the device that performs
them. The current implementation folders are:

- `src/oled/`: eye expressions, fixed reminder text, and monochrome art
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

| ID | Nominal duration | Behavior / successful final state |
| --- | --- | --- |
| `led.blink` | 1.2 s | Three red status-LED blinks, then off (existing) |
| `neopixel.rainbow` | 2.56 s | One rainbow cycle, then off (existing) |
| `neopixel.blink_red` | 1.2 s | Three red RGB blinks, then off |
| `neopixel.blink_yellow` | 1.2 s | Three yellow RGB blinks, then off |
| `neopixel.blink_green` | 1.2 s | Three green RGB blinks, then off |
| `oled.happy` | 2.88 s | 48 frames of happy eyes; retains last frame |
| `oled.sad` | 2.88 s | 48 frames of sad eyes with tears; retains last frame |
| `oled.surprised` | 2.88 s | 48 frames of wide eyes with small pupils; retains last frame |
| `oled.blink` | 0.54 s | One close/reopen cycle; retains open happy eyes |
| `oled.curious` | 2.88 s | Existing asymmetric curious expression; retains last frame |
| `oled.drink_water` | 2 s | Fixed two-line message; remains visible after return |
| `oled.water_drop` | 1.44 s | 24 gently moving drop frames; retains last frame |
| `oled.fire` | 1.44 s | 24 monochrome flame frames; retains last frame |
| `speaker.drink_water` | About 2.23 s | 1.73 s offline-generated phrase plus silence |
| `speaker.chirp` | About 0.82 s | Two rising bird-like synthesized chirps plus silence |
| `speaker.alert` | About 0.68 s | Short 660 Hz alert plus silence |

There are 16 actions, of which 14 are new. Curious eyes reuse an existing
renderer; the neutral alert is a small extra. No event bundles are registered.
The red-only status LED cannot make yellow or green; use the NeoPixel IDs.

## Execution contract

All actions have fixed parameters and `cancellable = false`. They block the
caller and must run serially. Durations are nominal, excluding initialization,
I/O overhead and scheduler rounding. Future protocol code should call them
from a serialized worker, not from its receive loop. Concurrent actions on the
same driver, cancellation, arbitrary text and dynamic names are not supported.

Initialize `tokki_board`, `tokki_led` and `tokki_neopixel` as production does.
The SSD1306 128x64 OLED must use address `0x3C` and the board's STEMMA QT pins.
Its driver initializes lazily on the first OLED frame; a failed initialization
releases acquired resources and returns the driver error so a later call can
retry. A successful initialization retains the bus/panel for subsequent actions.
Do not use this driver alongside the self-test's independent I2C owner. The
standalone diagnostic application remains separate and unchanged.

The MAX98357A uses the existing board-defined I2S pins. Speaker actions acquire
and release their I2S channel per call, use fade envelopes for tones, and retain
the hard 20% digital sample ceiling. This is not a guarantee of safe electrical
wattage: check gain strapping, supply, impedance, distortion and heat on hardware.

Actions stop on the first driver failure and return it. Final-state guarantees
apply only on success; failed LED writes cannot guarantee that the LED is off.
OLED art stays visible until another frame is drawn. No action starts an idle
loop or background animation task.

## Scope and verification

The requested OLED text/art extends the previous eyes-only contribution scope
and needs owner review. A realistic bark awaits an original/licensed asset;
`Hey <name>` awaits a parameter or fixed-name asset decision. Neither has a
placeholder registry entry. Desktop, USB command handling and personality are
not implemented by this contribution. Production still only logs the catalog;
hardware reviewers must explicitly invoke `tokki_action_run(id)` in a temporary
harness to exercise individual gestures.

On Windows with Visual Studio C++ tools:

```powershell
./tests/host/run.ps1
```

Host checks compile the actual registry, action wrappers, eye/art renderers and
tone generator with warnings as errors. They verify unique IDs, timing, output
bounds, driver-failure propagation, tone envelopes and the speech WAV format.
I/O is mocked: these tests do not compile or validate the ESP-IDF I2C/I2S drivers.

Optional export of actual C-rendered frames for an external preview:

```powershell
./tests/host/run.ps1 -PreviewPath ../gesture-preview.js
```

Before hardware approval, build both applications in an exported ESP-IDF shell:

```powershell
idf.py build
idf.py -C self_test -B self_test/build build
```

Neither ESP-IDF build nor physical hardware playback was available during the
initial host validation. Review remains blocked on those gates. Exercise every
ID, check the OLED missing-device path, repeat speaker playback to check channel
reuse, and rerun the original standalone self-test before approval.

## Checklist

1. Implement one finite action in the matching device folder.
2. Use a stable lowercase ID with the device prefix.
3. Return an `esp_err_t`; do not silently ignore a driver failure.
4. Register the descriptor in `tokki_gestures.c`.
5. Document parameters, duration, and cancellation behavior.
6. Build both the production firmware and the standalone self-test.
7. Exercise the action on hardware and note any required peripherals.