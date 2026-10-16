# CYD-Metal

Experimental **pulse-induction (PI) metal detector** using a classic ESP32 and the **CYD 3.5" (ESP32-3248S035, 480 × 320)** display/touch board. The immediate target is a large buried cast-iron/steel manhole cover beneath unreinforced concrete. The project draws on the Spirit PI *hardware concept*, but its firmware is written independently.

**Hardware status: 13 October 2026.** Firmware, touchscreen, TX switching stage and coil have been tested on real hardware. The **analog receiver is not yet complete or connected to GPIO35**. The current ADC/score values must not be interpreted as validated metal-detection performance.

## What works now

- ESP-IDF 5.4 firmware on classic ESP32; ST7796 UI and touchscreen.
- On-screen ZERO / MUTE / RAW-PERSIST controls; **SET** page for live TX pulse width, ADC tap offsets and moving average; AUTOZERO, score, EARLY / MID / LATE and logging.
- GPIO22 TX: **ESP32 RMT hardware-generated** selectable 25/50/75/100 µs HIGH and complementary LOW (**200 Hz**); **adaptive ADC acquisition**: for first tap ≤40 µs, one read per TX pulse over 48 pulses; for first tap ≥50 µs, all three tap readings after the same TX OFF over 16 pulses; each frame averages up to 16 reads per tap, without pausing TX; **three programmable ADC tap times t / 2t / 3t, with t selectable from 10 to 60 µs**, anchored to GPIO22 falling-edge readback. Additionally smooth 5 or 10 consecutive frames. This replaces interrupt-sensitive `gpio_set_level()` + `esp_rom_delay_us()` pulse widths.
- Discrete BJT gate driver (BC847A / BC807 / BC847A + BAT54), MOSFET from the 13NK60 600 V family, and a physical 210 mm coil; gate and drain observed on an oscilloscope.
- BAV99 receiver-input limiting network with a 15 kΩ resistor from DRAIN is physically assembled and tested.

## Touchscreen live controls (experimental)

Tap **SET** (top-right corner) to open the live tuning page:

- TX pulse duration: **25 / 50 / 75 / 100 µs**, with 5 ms total period (200 Hz). Changing TX width briefly stops/restarts the RMT hardware loop; selecting timing, smoothing or using the display does **not** stop TX.
- First ADC tap after **GPIO22 LOW**: **10 / 15 / 20 / 25 / 30 / 40 / 50 / 60 µs**. The other taps are automatically **2×** and **3×** that time (for example 10/20/30 µs or 30/60/90 µs).
- Frame moving average: **OFF / 5 / 10 frames** (OFF = current frame only). Each accepted frame aggregates up to **16 valid ADC readings per tap**, from 48 pulses (first tap ≤40 µs) or 16 pulses (first tap ≥50 µs) (only requests that fall in the timing window); 10 frames can combine up to 160 readings per tap, with slower response to target movement.
- **Settings persist in ESP32 NVS**: TX pulse width, first ADC tap and AVG OFF/5/10 are saved as one versioned record on touch changes and restored before RMT starts. No SAVE button required. Erasing flash/NVS or reflashing with a partition erase restores defaults.
- Changing acquisition settings clears the previous baseline and smoothing window; the next **valid** frame autozeros. Use ZERO to rebaseline with the coil stationary and target away.
- **Timing filter:** an ADC request is accepted only if its start-time estimate is from **t to t+1 µs**, relative to the software-observed GPIO22 falling edge; late requests are **discarded**. All three taps must have at least **8 accepted reads from their respective 16 scheduled TX pulses** or the entire frame is excluded from averaging, ZERO and score. The screen shows **ADC TIMING SKIP** rather than a fictitious zero. Valid readings use their actual per-tap accepted sample count as averaging divisor.
- The normal screen displays **OK a/b/c**, the accepted sample counts for the three taps (max 16 per tap), **LATE** the rejected samples, **MAX** the worst request-start lateness (µs) and **ADC** the maximum one-shot driver call duration (µs). The log also reports per-tap rejected samples. This is **not an exact hardware ADC sampling guarantee**.

**ADC scheduling fix:** On the actual board, the reported maximum `adc_oneshot_read()` duration was **39 µs**, and the original three-conversions-per-pulse capture yielded `VALID 16/16 0/16 0/16` with `MAX LATE 30US`. A sequential read cannot meet 10/20/30 µs timings when a preceding read takes up to 39 µs. For first-tap offsets **10–40 µs**, each TX pulse schedules exactly one ADC conversion: successive pulses sample t, 2t, and 3t. The complete 16-reading-per-tap frame uses 48 pulses (about **240 ms**). For first-tap offsets **50 or 60 µs**, there is more spacing between the three target times than the observed maximum ADC call duration (39 µs), so all three ADC reads are attempted after **the same TX OFF**. A complete frame then uses 16 pulses (about **80 ms**). Timing-inaccurate readings continue to be discarded by the existing +1 µs request-start filter, and frames with too few accepted readings are skipped. **50 µs is experimental**: if overhead or interrupt delays cause missed deadlines, use the interleaved ≤40 µs setting or a later tap. AVG OFF presents the latest frame only; AVG 5 and AVG 10 smooth 5 or 10 frames, respectively. The display and serial log identify the active `1/TX` or `3/TX` mode.

**Synchronization limitation:** RMT controls TX edges in hardware but GPIO22 falling-edge detection and ADC1 oneshot reads are still polled in software (one or three reads per pulse depending on settings). At **10/20/30 µs**, FreeRTOS preemption and ADC conversion duration may cause late readings. Rejecting calls whose **request start** is more than 1 µs late avoids mixing obviously late values, but an interrupt after the check or the internal ADC sampling aperture can still shift the true acquisition time. A future hardware-triggered path is required to guarantee the sampling instant. The transistor driver/DRAIN turn-off may occur ~2 µs **after** GPIO22 LOW, so the taps are not referenced to the true DRAIN switching edge. Hardware-timed ADC capture remains future work.

The modifications are **not yet compiled/flashed/validated** on the physical CYD; test the new controls first with the TX power supply disconnected and a scope on GPIO22/GATE.

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

The already-tested **DRAIN → 15 kΩ → BAV99-limited RX node** is being AC-coupled into a **3.3 V MCP602** inverting amplifier, with a passive and decoupled **1.65 V reference**. In bench trials the RX has used 47 kΩ input resistance, 180 kΩ feedback and antiparallel feedback diodes; the coupling capacitor was changed from 3.5 nF to **85 nF**. The best gain/recovery settings remain experimental. A provisional output network is **1 kΩ series to GPIO35, 1 nF to ground**, subject to validation and input safety checks.

**Do not connect GPIO35 yet.** Test the RX output with an oscilloscope during switching, power-up, power-down and target approach; ensure ADC-safe voltage for all operating conditions.

See [hardware measurements and architecture](docs/HARDWARE.md), [TX BJT driver](docs/DISCRETE_BJT_DRIVER.md), [MCP602 receiver plan](docs/RX_MCP602.md), [bring-up procedure](docs/BRINGUP.md) and [roadmap](docs/ROADMAP.md).

## RMT timing change (13 October 2026)

Oscilloscope measurements with software GPIO pulses showed widths varying between **100.45 µs and 104.39 µs**; the resulting TX-current variability injected noise into RX. The code now uses ESP-IDF 5.4 `esp_driver_rmt` to **loop a single 5 ms TX symbol indefinitely** at 1 MHz resolution, so both the 100 µs HIGH and 4,900 µs LOW durations remain hardware-clocked even during display updates.

The capture task enables GPIO22 **input readback alongside the RMT output**, waits for its actual rising/falling transitions, and anchors ADC taps to the observed TX-OFF edge. RMT is started once after setup and never stopped between 16-sample frames; only a fatal acquisition error stops TX. The task can still be preempted during ADC sampling: the existing `late_samples` diagnostic is retained. **The user confirmed stable RMT TX pulse widths with a scope.** The ADC windows were subsequently revised to 30/60/90 µs; they have not yet been tested on the physical board.

**Bench verification for continuous mode (not yet tested):** after flashing, scope GPIO22/GATE at 20 µs/div for pulse width (expect about 100 µs), then at 100–200 ms/div and confirm absolutely no gaps between pulse trains while the CYD updates the LCD. Also confirm 5 ms spacing and stable receiver idle level. Observe the analog RX again at 30, 60 and 90 µs after TX OFF. If capture fails with an ESP_ERR_TIMEOUT, first verify GPIO22 readback during RMT output and check that the TX signal is electrically high for the entire pulse.

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
