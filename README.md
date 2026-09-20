# Physical Tokki

Physical Tokki is an ESP32-based desktop pet with OLED eye expressions,
speaker notifications, a status LED, and NeoPixel effects. The repository is
being organized so contributors can add gestures independently of the board
diagnostics and desktop integrations.

## Repository layout

```text
components/tokki_board/       Feather V2 pins and peripheral power
components/tokki_led/         Onboard red LED driver
components/tokki_neopixel/    RGB color and rainbow effects
components/tokki_oled/        OLED eye renderer
components/tokki_speaker/     I2S speaker code and embedded audio
components/tokki_gestures/    Discoverable actions grouped by device
components/tokki_runtime/     Serial protocol, action queue, and idle eyes
main/                         Production firmware
self_test/                    Standalone hardware self-test firmware
protocol/                     PC-to-pet serial protocol
pc-app/                       Tauri desktop app for discovery/manual playback
docs/                         Architecture documentation
```

See [the architecture](docs/architecture.md), [gesture contribution guide](components/tokki_gestures/README.md), and [serial protocol](protocol/tokki-serial-v1.md).

## Production firmware

The root ESP-IDF application initializes the shared board, LED, and NeoPixel,
serves the gesture registry over USB serial, and accepts manual playback
commands from the desktop app. Each device has its own worker, with four
shared waiting queue slots. While idle, the OLED shuffles blinks, glances,
happy, curious, lovey-dovey, shy, and a paired sleeping-eyes-to-night-sky
sequence, with 0.6-1.2 seconds of calm open eyes between choices. All nine
choices get a turn before reshuffling;
the same shuffled choice never plays twice in a row. Each left/right glance
also finishes with a soft 0.54-second blink before the calm pause.
Night sky only follows sleeping eyes with floating Zzz, immediately and without
an intervening open-eye pause. Each part runs for 2.88 seconds at the idle
60 ms frame interval; the eyes stay closed until the sky starts, then return
to open eyes after the sky. Sunrise remains a manual gesture.

Independently, the NeoPixel plays occasional 1.98-second teal breaths with
6-14-second dark pauses, capped at 32/255 per green/blue channel. This reuses
the manual breathing gesture's fade rather than flashing or adding sound.
Incoming gestures take priority on their device after the current hardware
write; other devices' idle animations can continue. Idle resumes with a calm
hold or dark pause after that device's queue drains, even without a PC
connection. OLED and speaker hardware are initialized on first use.

Manually triggered OLED, status LED, and RGB gestures now run 50% longer
than the initial prototype, with unchanged frame counts, brightness, and
final states. Idle frames still use 60 ms; speaker sounds and self-test timings
are unchanged.
See the [gesture duration table](components/tokki_gestures/README.md#current-actions).

The onboard **red status LED (GPIO13)** latches on after a startup failure,
a failed gesture/idle hardware operation (including lazy OLED/speaker
initialization), or a detected firmware serial I/O failure. It stays on until
the pet resets, even if a later retry succeeds. Fault indication takes priority
over `led.blink`: that action still completes its timing but cannot turn the
LED off while a fault is latched. Healthy startup leaves it off.
The RGB NeoPixel remains available for gestures and is not the fault indicator.
If the status LED's own initialization or GPIO writes fail, illumination cannot
be guaranteed; that failure is logged. Invalid PC requests or a PC-side
COM-port access error do not latch a firmware hardware fault.

From an exported ESP-IDF shell:

```powershell
idf.py build
idf.py flash monitor
```

## End-to-end prototype

1. Connect the Feather V2 over USB, with peripherals wired as described below.
2. Build and flash the **root production application**, not `self_test/`.
3. Stop the ESP-IDF monitor so the desktop app can open the serial port.
4. Start the [native desktop app](pc-app/README.md) with
   `npm run tauri dev` from `pc-app/`.
5. Select the Feather's COM port and connect. The app handshakes with the
   firmware and automatically fetches every page of its registered gestures.
6. Use the Gestures screen to send individual actions. Acceptance, running,
   completion, and failures come from the physical device, not UI timers.
   Additional actions queue in order; a full queue reports Busy.
7. Let the queue finish or disconnect the app: the pet returns to idle eyes
   and intermittent teal breathing.
   Disconnect does not cancel gestures already accepted by the firmware.

Serial uses 115200 baud, 8N1, with no flow control. Discovery means reading
gesture descriptors from the selected pet, not probing every COM device.
No event-to-gesture rules, email integration, arbitrary text, cancellation,
Wi-Fi, or Bluetooth are implemented in this prototype.

The catalog describes firmware capabilities, not verified peripheral
presence. A missing OLED produces a visible idle/gesture error and retries
idle drawing every five seconds; other gestures remain usable. Software
timing and host tests do not replace hardware checks.

### Prototype validation

After building production firmware once to restore its managed cJSON
dependency, run:

```powershell
.\tests\host\run.ps1 -Runtime
Set-Location .\pc-app
npm run build
cargo test --manifest-path .\src-tauri\Cargo.toml
```

The host suite covers all 47 gestures, including lovey-dovey, shy and sleeping eyes,
twinkling night sky and sunrise scenes,
the unchanged twice-repeated bark and the four clean self-test-frequency tones,
and additionally checks
protocol framing, complete paginated discovery, bounded per-device FIFO
execution, cross-device dispatch, lifecycle events, malformed input recovery,
shuffled idle eye/scene coverage, sleeping/night-sky preemption, breathing brightness/pause bounds, and
worker preemption/deadline handling (including timer wraparound).
The UART/FreeRTOS adapter and actual peripherals still need an on-device
smoke test: observe several shuffled eye rounds and breathing pauses, send
OLED/NeoPixel gestures during idle (including during a dark pause), fill the
queue, unplug/reconnect the PC, and verify each device resumes idle without
concurrent writes.

## Hardware self-test

The independent `self_test/` application performs a safe hardware smoke test
for the Adafruit ESP32 Feather V2:

- Prints chip, flash, heap, and Wi-Fi MAC information.
- Blinks the red LED on GPIO13 three times.
- Cycles red, green, and blue five times on the onboard NeoPixel on GPIO0.
- Runs five smooth full-spectrum rainbow cycles on the NeoPixel after the
  red/green/blue sequence.
- Enables the NeoPixel/STEMMA QT power rail on GPIO2.
- Optionally scans for nearby Wi-Fi access points; disabled by default.
- Plays three short, ramped MAX98357A speaker tones with every generated audio
  sample hard-limited to 20% of digital full scale.
- Scans the STEMMA QT I2C bus on SDA GPIO22 and SCL GPIO20.
- Detects an optional SSD1306 128x64 OLED at I2C address 0x3C.
- When detected, uses a procedural pet-eye renderer for large happy, sad, and
  curious eyes. Tall oval whites, smiling crescent closures, drooping lids,
  asymmetric squash/stretch, and pupil tracking convey emotion without
  eyebrows or falling tears. See the [eye language and timing](components/tokki_gestures/src/oled/README.md#eye-language-and-frame-contract).
  It then scrolls `Hello Ram, Hiten, Shyam` horizontally in a large single-line
  font.
- Briefly illuminates the top 16 rows and lower 48 rows separately to reveal
  whether the physical OLED has yellow/blue color zones.

The board self-test passes without an OLED. If no display is detected, the
serial monitor reports the OLED stage as `[SKIP]`.

Wi-Fi scanning is disabled by default, and no Bluetooth scan is performed.
The chip-information line only reports whether the ESP32 supports BT/BLE; it
does not turn on or scan with the Bluetooth radio. To enable Wi-Fi scanning,
open `ESP-IDF: SDK Configuration Editor`, find `Feather V2 Self-Test`, and
enable `Wi-Fi access-point scan`.

## MAX98357A speaker test

The speaker test uses:

| MAX98357A | Feather V2 |
| --- | --- |
| BCLK | GPIO27 |
| LRC / WS | GPIO33 |
| DIN | GPIO32 |
| VIN | USB / 5V |
| GND | GND |

It plays 440 Hz, 660 Hz, and 880 Hz for 300 ms each, with silence between
tones and 25 ms fade-in/fade-out envelopes to reduce clicks. The software
amplitude cannot exceed 20% of digital full scale. After the tones, it plays
an embedded offline-generated voice saying `Hello Ram`; speech samples are
also scaled to the same hard 20% ceiling.

Software amplitude does not directly guarantee electrical output wattage;
MAX98357A gain strapping, supply voltage, speaker impedance, and module design
also affect power. Stop the test if the speaker distorts, clicks heavily, or
gets warm. The speaker test can be disabled under `Feather V2 Self-Test` in
the ESP-IDF SDK Configuration Editor.

## OLED connection

Connect the SSD1306 OLED to the Feather V2 STEMMA QT connector, or wire:

| OLED | Feather V2 |
| --- | --- |
| VCC | 3V |
| GND | GND |
| SDA | SDA / GPIO22 |
| SCL | SCL / GPIO20 |

The display must be configured for I2C address `0x3C`.

## Run the self-test

1. Connect the Feather V2 over USB-C.
2. Open an exported ESP-IDF shell at the repository root.
3. Build with `idf.py -C self_test -B self_test/build build`.
4. Flash and monitor with `idf.py -C self_test -B self_test/build flash monitor`.

The serial monitor reports `[PASS]` or `[FAIL]` for each stage. A successful
test leaves the NeoPixel green and flashes the red LED briefly every two
seconds. A failed test leaves the NeoPixel red. When an OLED is connected, the
two color-zone screens, three animated eye expressions, and scrolling message
repeat continuously after the other tests finish.
