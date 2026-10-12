# CYD-Metal

Experimental **pulse-induction (PI) metal detector** using a classic ESP32 and the **CYD 3.5" (ESP32-3248S035, 480 × 320)** display/touch board. The immediate target is a large buried cast-iron/steel manhole cover beneath unreinforced concrete. The project draws on the Spirit PI *hardware concept*, but its firmware is written independently.

**Hardware status: 13 October 2026.** Firmware, touchscreen, TX switching stage and coil have been tested on real hardware. The **analog receiver is not yet complete or connected to GPIO35**. The current ADC/score values must not be interpreted as validated metal-detection performance.

## What works now

- ESP-IDF 5.4 firmware on classic ESP32; ST7796 UI and touchscreen.
- On-screen ZERO / MUTE / RAW-PERSIST controls; AUTOZERO, score, EARLY / MID / LATE and logging.
- GPIO22 TX: 100 µs pulse every 5 ms (200 Hz); 16 pulses averaged; five ADC tap times: 40 / 80 / 130 / 200 / 300 µs after TX OFF.
- Discrete BJT gate driver (BC847A / BC807 / BC847A + BAT54), MOSFET from the 13NK60 600 V family, and a physical 210 mm coil; gate and drain observed on an oscilloscope.
- BAV99 receiver-input limiting network with a 15 kΩ resistor from DRAIN is physically assembled and tested.

## Bench measurements (not design estimates)

| Item | Observed value / status |
|---|---|
| Coil | 210 mm diameter, 30 turns, ~0.19 mm enamelled copper wire |
| Coil DC resistance | 12.4 Ω |
| Estimated inductance | ~364 µH, based on an external-capacitor resonance test |
| Natural resonance | ~824 kHz with x10 scope probe |
| Estimated effective parasitic capacitance | ~103 pF including wiring and probe |
| TX rail during drain tests | 23 V |
| Damping resistor presently installed | **150 Ω across coil** |
| DRAIN peak at 23 V, with 75 Ω (older test) | ~145 V |
| DRAIN peak at 23 V, with **150 Ω (current)** | **~250 V** |
| Separate drain TVS/clamp | **Not fitted; still to be designed** |

The TX driver is currently supplied from the TX rail (not a dedicated 12 V supply). Gate voltage reached approximately 23 V during tests; actual VGS, including transients, must remain within the **exact installed MOSFET's** datasheet limit. The finished portable unit is intended to use a nominal 18 V Makita battery (~21 V fully charged).

## Work in progress: MCP602 analog receiver

The already-tested **DRAIN → 15 kΩ → BAV99-limited RX node** will be AC-coupled into a **3.3 V MCP602** inverting amplifier, with a passive and decoupled **1.65 V reference**. Gain, coupling capacitor, input clamps and recovery behavior must still be determined experimentally. A provisional output network is **1 kΩ series to GPIO35, 1 nF to ground**, subject to validation and input safety checks.

**Do not connect GPIO35 yet.** Test the RX output with an oscilloscope during switching, power-up, power-down and target approach; ensure ADC-safe voltage for all operating conditions.

See [hardware measurements and architecture](docs/HARDWARE.md), [TX BJT driver](docs/DISCRETE_BJT_DRIVER.md), [MCP602 receiver plan](docs/RX_MCP602.md), [bring-up procedure](docs/BRINGUP.md) and [roadmap](docs/ROADMAP.md).

## CYD pin profile

| Function | GPIO |
|---|---:|
| PI TX pulse | 22 |
| Conditioned RX ADC (not connected yet) | 35 / ADC1_CH7 |
| Audio amplifier input | 26 |
| RGB red / green / blue | 4 / 16 / 17 |
| BOOT / ZERO | 0 |
| TFT backlight | 27 |

## Build

Requirements: ESP-IDF **5.4** as used in the current project, classic ESP32 target.

```bash
git clone https://github.com/eugentib/CYD-Metal.git
cd CYD-Metal
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

On Linux replace `COMx` with the serial device (for example `/dev/ttyUSB0`).

## Next priorities

1. Assemble and characterize the MCP602 analog RX signal chain **without connecting the ESP32 ADC**.
2. Design a real MOSFET overvoltage clamp with headroom for the planned lower-resistance coil; verify repetitive pulse energy and safe voltage.
3. Validate metal response at controlled distances; then connect conditioned RX to GPIO35 and tune sampling.
4. Optionally wind a ~0.5 mm wire coil and compare field strength, decay and losses.

Until the receiver is characterized, this remains an experimental instrument, **not a verified underground utility locator**.
