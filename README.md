# CYD-Metal

Experimental pulse-induction metal detector firmware for a **CYD 3.5" ESP32-3248S035** connected to a Spirit PI style analog/TX front-end.

The project deliberately reuses the proven Spirit PI hardware concept but **does not reuse the original firmware**. The acquisition, signal processing, UI and feedback stack are being written from scratch.

## Current milestone: M0 / bring-up

The first firmware is intentionally display-free. It focuses on the part that matters most for a PI detector:

- deterministic TX pulse generation;
- five ADC samples from the decay after TX turns off;
- multi-pulse averaging;
- ZERO/baseline capture;
- EARLY / MID / LATE response values;
- a simple target score and persistence metric;
- CYD RGB LED and speaker feedback;
- serial logging for oscilloscope-assisted tuning.

Once the analog timing is stable we will replace the five-tap ADC acquisition with continuous/DMA capture and then add the ST7796 display/touch UI.

## Assumed CYD board

Sunton-style **ESP32-3248S035R/C**, classic ESP32-WROOM-32.

Current pin profile:

| Function | GPIO |
|---|---:|
| PI pulse output | 22 |
| PI analog input | 35 / ADC1_CH7 |
| Onboard audio amplifier | 26 |
| RGB red | 4 |
| RGB green | 16 |
| RGB blue | 17 |
| BOOT / ZERO button | 0 |
| TFT backlight | 27 |

GPIO35 is input-only and is exposed on P3. GPIO22 is also exposed on P3.

## Wiring for M0

CYD P3 to detector front-end:

- **GND -> GND**
- **GPIO22 -> Spirit PI pulse input**
- **GPIO35 -> Spirit PI analog output**

Power the Spirit PI analog/TX front-end from its own appropriate supply. Power the CYD from 5 V. Grounds must be common.

**Before connecting GPIO35, verify with an oscilloscope that the analog output is always within the ESP32 ADC range and never contains the MOSFET drain flyback pulse.**

## Build

Requirements:

- ESP-IDF 5.2 or newer
- target: classic ESP32

```bash
git clone https://github.com/eugentib/CYD-Metal.git
cd CYD-Metal

idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

On Linux, replace `COMx` with the serial device such as `/dev/ttyUSB0`.

## First test

1. Flash the CYD with the detector front-end disconnected.
2. Confirm serial boot and RGB feedback.
3. Connect common GND.
4. Connect GPIO22 to the pulse input and inspect the pulse with an oscilloscope.
5. Only after checking the conditioned analog output, connect it to GPIO35.
6. With no metal near the coil, press **BOOT** briefly to ZERO.
7. Move a large steel object toward the coil and observe the five ADC taps and score in the serial log.

See [docs/BRINGUP.md](docs/BRINGUP.md) for the detailed procedure.

## Design direction

The detector core is independent from the future UI. The same processing code should later build for:

- CYD 3.5" with display/touch/audio;
- headless ESP32 + NeoPixel + speaker;
- other ESP32 boards with a compatible ADC/TX interface.

## Status

Prototype software. Do not treat the current score or "persistence" metric as metal identification; they are diagnostics until we collect real waveforms from the target and the reinforced concrete environment.
