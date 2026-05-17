# KB2040 USB MIDI Stompbox Controller

Firmware for a six-switch USB MIDI stompbox built around an Adafruit KB2040.
The controller sends MIDI CC messages for six stomp switches, shows status on a
128x64 SSD1306 OLED, and includes an onboard edit mode for changing each
switch's HX Stomp assignment without reflashing the firmware.

## Board Target

- Board: Adafruit KB2040
- Arduino core: Raspberry Pi Pico/RP2040/RP2350 by Earle F. Philhower, III
- FQBN: `rp2040:rp2040:adafruit_kb2040:usbstack=tinyusb`
- USB stack: Adafruit TinyUSB

The KB2040 uses RP2040 GPIO numbers as Arduino pin numbers. For example,
Arduino pin `4` is GPIO4 / the board pad labeled `D4`, not an AVR Pro Micro
pin remap.

## Behavior

- MIDI channel: `1`
- Press debounce: `25 ms`
- Edit mode hold: `4 seconds`
- Pressing a stomp switch sends one USB MIDI Control Change message.
- The last-button display returns to the home screen after `5 seconds`.
- Holding a stomp switch enters edit mode for that switch after the initial
  normal MIDI message.
- Pressing the same stomp switch while editing exits edit mode.
- Previous/next edit buttons scroll the HX Stomp preset list.
- Edited assignments are saved using the RP2040 core's flash-backed
  `EEPROM.h` emulation.

## Button Wiring

Wire each button between the listed GPIO pin and `GND`. The firmware uses
`INPUT_PULLUP`, so a pressed button reads `LOW`.

| Button | KB2040 GPIO / Arduino Pin | Default MIDI CC | Press Value |
| --- | ---: | ---: | ---: |
| 1 | 4 | 69 | 0 |
| 2 | 5 | 69 | 1 |
| 3 | 6 | 69 | 2 |
| 4 | 7 | 52 | 127 |
| 5 | 8 | 53 | 127 |
| 6 | 9 | 25 | 127 |

## Edit Buttons

| Edit Control | KB2040 GPIO / Arduino Pin | Behavior |
| --- | ---: | --- |
| Previous | 10 | Previous assignable preset |
| Next | A0 / GPIO26 | Next assignable preset |

## Display Wiring

The sketch uses `Wire1`, the KB2040's secondary I2C bus on visible edge pins:

| I2C Signal | KB2040 Pin |
| --- | --- |
| SDA | D2 |
| SCL | D3 |

Use a 128x64 monochrome SSD1306 I2C display. The firmware uses U8g2 full-buffer
graphics mode for pixel-positioned text, frames, and inverse headers.

## Build

Install the Philhower RP2040 board package URL in Arduino IDE / Arduino CLI:

```text
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```

Then compile with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

Equivalent Arduino CLI command:

```powershell
arduino-cli compile --fqbn rp2040:rp2040:adafruit_kb2040:usbstack=tinyusb KB2040MIDIController
```

In Arduino IDE, select **Adafruit KB2040** and set **Tools > USB Stack** to
**Adafruit TinyUSB**.

## Startup Screen

```text
USB MIDI Ctrl

Button Layout

1  2  3
4  5  6

MIDI Channel: 1
```
