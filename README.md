# Adafruit ESP32 Feather V2 Self-Test

This ESP-IDF project performs a safe hardware smoke test for the Adafruit
ESP32 Feather V2:

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
  curious eyes. It animates pupil tracking, catchlights, eyelids, natural
  blinks, thick curved moving eyebrows, asymmetry, saccades, and staggered
  falling teardrops, then scrolls `Hello Ram, Hiten, Shyam` horizontally in a
  large single-line font.
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

## Run

1. Connect the Feather V2 over USB-C.
2. In VS Code, run `ESP-IDF: Select Port to Use`.
3. Run `ESP-IDF: Set Espressif Device Target` and select `esp32`.
4. Run `ESP-IDF: Build, Flash and Start a Monitor`.

The serial monitor reports `[PASS]` or `[FAIL]` for each stage. A successful
test leaves the NeoPixel green and flashes the red LED briefly every two
seconds. A failed test leaves the NeoPixel red. When an OLED is connected, the
two color-zone screens, three animated eye expressions, and scrolling message
repeat continuously after the other tests finish.
