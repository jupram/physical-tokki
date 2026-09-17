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
| `led.blink` | 1.8 s | Three red status-LED blinks, then off (existing) |
| `neopixel.rainbow` | 3.84 s | One rainbow cycle, then off (existing) |
| `neopixel.blink_red` | 1.8 s | Three red RGB blinks, then off |
| `neopixel.blink_yellow` | 1.8 s | Three yellow RGB blinks, then off |
| `neopixel.blink_green` | 1.8 s | Three green RGB blinks, then off |
| `neopixel.breathe_teal` | 1.98 s | One slow teal fade in/out, then off |
| `neopixel.pulse_blue` | 1.02 s | One quick blue fade in/out, then off |
| `oled.happy` | 4.32 s | 48 frames of happy eyes; retains last frame |
| `oled.sad` | 4.32 s | 48 frames of sad eyes with tears; retains last frame |
| `oled.surprised` | 4.32 s | 48 frames of wide eyes with small pupils; retains last frame |
| `oled.blink` | 0.81 s | One close/reopen cycle; retains open happy eyes |
| `oled.curious` | 4.32 s | Existing asymmetric curious expression; retains last frame |
| `oled.drink_water` | 3 s | Fixed two-line message; remains visible after return |
| `oled.water_drop` | 2.16 s | 24 gently moving drop frames; retains last frame |
| `oled.fire` | 2.16 s | 24 monochrome flame frames; retains last frame |
| `oled.wink` | 0.90 s | One eye closes/reopens; restores centered happy eyes |
| `oled.checkmark` | 1.53 s | Draws a check, holds, then restores happy eyes |
| `oled.thinking` | 1.80 s | Three dots advance once, then restores happy eyes |
| `oled.look_left` | 2.16 s | Brief leftward glance, then centered happy eyes |
| `oled.look_right` | 2.16 s | Brief rightward glance, then centered happy eyes |
| `oled.look_up` | 2.16 s | Brief upward glance, then centered happy eyes |
| `oled.look_down` | 2.16 s | Brief downward glance, then centered happy eyes |
| `oled.sleepy` | 2.16 s | Lids lower/reopen, then restores happy eyes |
| `oled.heart` | 2.16 s | Small heart pulse, then restores happy eyes |
| `oled.exclamation` | 1.53 s | Exclamation mark grows into view, then restores happy eyes |
| `speaker.drink_water` | About 2.23 s | 1.73 s offline-generated phrase plus silence |
| `speaker.chirp` | About 0.82 s | Two rising bird-like synthesized chirps plus silence |
| `speaker.alert` | About 0.68 s | Short 660 Hz alert plus silence |
| `speaker.chime` | About 0.96 s | Two rising notes (400 ms audible), 60 ms gap, 500 ms silence |
| `speaker.ping` | About 0.60 s | One 100 ms, 1320 Hz ping plus 500 ms silence |
| `speaker.dog_bark` | About 1.02 s | User-provided recording repeated twice (0.519875 s), plus 500 ms silence |
| `speaker.bubble` | About 0.59 s | Synthetic 90 ms falling pop, 1200 to 480 Hz, plus silence |
| `speaker.whistle` | About 0.78 s | Synthetic 280 ms rising whistle, 900 to 2100 Hz, plus silence |
| `speaker.sigh` | About 0.86 s | Synthetic 360 ms falling sigh, 850 to 350 Hz, plus silence |
| `speaker.boing` | About 0.74 s | Synthetic rising/falling bounce, 240 ms audible, plus silence |
| `speaker.question` | About 0.82 s | Note and upward inflection, 270 ms audible plus gap/silence |
| `speaker.downstep` | About 0.86 s | Gentle two-note descent, 320 ms audible plus gap/silence |
| `speaker.sparkle` | About 0.83 s | Three ascending notes, 270 ms audible plus gaps/silence |
| `speaker.trill` | About 0.76 s | Three quick chirrups, 210 ms audible plus gaps/silence |
| `speaker.knock` | About 0.73 s | Two synthetic knocks, 130 ms audible plus gap/silence |
| `speaker.sonar` | About 0.85 s | Two pings, second at 40% relative gain, plus gap/silence |
| `speaker.bark` | About 1.00 s | One 0.5 s CC0 recorded bark excerpt plus silence |

There are 42 actions (18 OLED, 6 NeoPixel, 1 status LED, 17 speaker), of which 40
are new relative to the initial scaffold. This sound follow-up adds ten
synthetic effects and one CC0 recorded bark to main's 31-action catalog.
Both existing bark IDs are preserved: `speaker.dog_bark` uses the upstream
double recording; `speaker.bark` uses a separate CC0 single-bark excerpt.
No event bundles are registered. Glances are expressions,
not sensor tracking. Thinking dots are a finite cue, not actual progress or a
listening indicator. Only a future PC rule should decide whether work is pending
or completed. Checkmark and chime remain independent actions.
The red-only status LED cannot make yellow or green; use the NeoPixel IDs.

## Execution contract

All 25 manually triggered OLED, status LED, and NeoPixel gestures run 50%
longer than the initial prototype. Frame/hold delays are scaled, preserving
the original frame counts, colors, brightness, and successful final states.
OLED frames use 90 ms; light blinks use 300 ms on/off; RGB fades use 60 ms;
rainbow uses 30 ms per step. The idle-eye loop, speaker sounds, and standalone
self-test timing are unchanged. The default rainbow driver API keeps its
20 ms steps; the gesture uses the timed variant.

All actions have fixed parameters and `cancellable = false`. They block the
caller and must run serially per physical device. Durations are nominal,
excluding initialization, I/O overhead and scheduler rounding. Production
protocol code calls them from per-device workers, not from its receive loop.
Different physical devices can run concurrently; concurrent actions on the
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
The first eight OLED actions retain their final frame; new transient OLED
actions explicitly restore centered happy eyes. No action starts an idle loop
or background animation task. The separate production runtime resumes idle
eyes after all queued actions finish, replacing the last OLED frame at that
point. RGB fades and blinks are capped at 32/255 per
channel, matching the existing rainbow brightness limit.

Production hardware faults latch the onboard red status LED on until reset.
This takes priority over `led.blink` and its normal off final state: the gesture
retains its nominal timing but the LED stays on if a fault was latched.
Successful retries and later gestures do not clear the fault indication.

## Scope and verification

The requested OLED text/art extends the previous eyes-only contribution scope
and needs owner review. `speaker.dog_bark` uses a user-provided recording;
see [its provenance and redistribution caveat](src/speaker/README.md#dog-bark-asset-provenance).
`speaker.bark` uses a different CC0 recording; see
[the separate source details](src/speaker/README.md#dog-bark-recording-and-provenance).
`Hey <name>` awaits a parameter
or fixed-name asset decision and has no placeholder registry entry.
The production runtime and desktop app now support
USB serial discovery and manual playback of all registered IDs, plus a simple
idle-eye sequence. Event scheduling and personality rules remain out of scope.
Use the desktop Gestures screen to exercise individual actions on hardware;
see [the serial protocol](../../protocol/tokki-serial-v1.md).

On Windows with Visual Studio C++ tools:

```powershell
./tests/host/run.ps1
```

Host checks compile the actual registry, action wrappers, eye/art renderers and
tone generator with warnings as errors. They verify unique IDs, timing, output
bounds, driver-failure propagation, restored-eye frames, one-eye wink closure,
directional glances, RGB fades, fixed tone sequences, envelopes and WAV format.
Sound tests also check monotonic rising/falling frequency bounds, preserve the
original rising-sweep frequencies, and cap new sound durations including silence
at 1.5 seconds. This is a maximum, not padding to a minimum sound length.
I/O is mocked: these tests do not compile or validate the ESP-IDF I2C/I2S drivers.

Optional export of all 42 catalog entries for an external preview:

```powershell
./tests/host/run.ps1 -PreviewPath ../speaker-preview.js
```

The export captures actual OLED frames, new RGB writes and limited PCM tone
samples with the updated gesture durations. Status blink/rainbow export metadata
only and are labeled illustrative in the external guide. No guide or generated
preview asset is shipped inside this repository.

Before hardware approval, build both applications in an exported ESP-IDF shell:

```powershell
idf.py build
idf.py -C self_test -B self_test/build build
```

Neither ESP-IDF build nor physical hardware playback was available during the
initial host validation. Review remains blocked on those gates. Exercise every
ID, check the OLED missing-device path, repeat speaker playback to check channel
reuse, and rerun the original standalone self-test before approval.

On 2026-09-16 Ram reported full integration testing of the merged 30-action
catalog. That report does not validate the eleven new sounds in this follow-up.
They still need both firmware builds and a hardware listening check.

## Checklist

1. Implement one finite action in the matching device folder.
2. Use a stable lowercase ID with the device prefix.
3. Return an `esp_err_t`; do not silently ignore a driver failure.
4. Register the descriptor in `tokki_gestures.c`.
5. Document parameters, duration, and cancellation behavior.
6. Build both the production firmware and the standalone self-test.
7. Exercise the action on hardware and note any required peripherals.