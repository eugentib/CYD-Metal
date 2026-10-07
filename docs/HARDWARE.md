# Hardware interface

## Scope

M0 assumes the analog and high-current pulse section is based on the Spirit PI ESP32 hardware concept, while the controller is the CYD 3.5 board.

No original Spirit PI software is used.

## CYD 3.5 ESP32-3248S035R/C pins

Relevant pins for this project:

| Signal | GPIO | Notes |
|---|---:|---|
| TX pulse | 22 | Exposed on P3, output capable |
| Analog decay | 35 | Exposed on P3, input-only, ADC1_CH7 |
| Speaker amplifier input | 26 | Onboard FM8002A path |
| RGB red | 4 | active-low |
| RGB green | 16 | active-low |
| RGB blue | 17 | active-low |
| BOOT / ZERO | 0 | active-low; do not hold during reset unless flashing |
| TFT backlight | 27 | reserved for later UI |

P3 on common boards exposes GND, GPIO35, GPIO22 and GPIO21.

## Power

Recommended during development:

```text
12 V source
   |
   +---- Spirit PI analog/TX section
   |
   +---- buck 12 -> 5 V ---- CYD
             |
            GND
```

Use a common ground.

Do not run the CYD from a 7805 dropping 12 V unless you have verified regulator dissipation and display/backlight current.

## ADC protection requirement

GPIO35 must only see the **conditioned receiver output**, never the MOSFET drain or coil node.

Before connecting the CYD:

1. Run the front-end with the ADC output disconnected.
2. Observe the output with an oscilloscope.
3. Confirm minimum and maximum voltage stay inside the ESP32 ADC-safe range under:
   - no target;
   - large steel target close to the coil;
   - power on/off;
   - TX pulse start/stop.
4. Only then connect GPIO35.

## Initial coil

For first software bring-up, use the known-working Spirit PI style coil rather than changing coil geometry and firmware simultaneously.

After M0 works, the project will test larger coils aimed specifically at a large buried manhole cover.
