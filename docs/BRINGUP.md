# CYD-Metal bring-up and RX validation

**Updated 2026-10-13.** The CYD firmware, physical TX coil, BJT gate driver and BAV99 protection are already operating on the bench. The **next milestone is RX**, not a fresh TX build. Do not connect GPIO35 yet.

## Bench baseline already established

- CYD ST7796 and touchscreen operate; GPIO22 drives the discrete BJT gate stage.
- RMT-based TX firmware: 100 µs HIGH, 4,900 µs LOW (200 Hz) in an **infinite hardware loop**; ADC averages 16 cycles without pausing TX. **TX width was verified stable on the Rigol before switching to infinite-loop mode; verify gap-free continuous operation again.**
- Air-core 210 mm, 30 turns, 12.4 Ω, estimated ~364 µH.
- Damping across coil **150 Ω**, replacing earlier 75 Ω.
- At 23 V TX, oscilloscope DRAIN peaks: ~145 V with older 75 Ω; **~250 V with 150 Ω**.
- BAV99 + **15 kΩ from DRAIN** RX limiter assembled and tested.
- There is **no separate installed drain TVS/clamp**.
- MCP602 analog RX is on the bench with **Rin 47 kΩ, Rf 180 kΩ, coupling C 85 nF** and antiparallel feedback limiter diodes; recovery is being tuned. **GPIO35 must remain disconnected until the signal is validated safe.**

See [hardware log](HARDWARE.md) and [gate driver](DISCRETE_BJT_DRIVER.md) for more detail.

## Before RX assembly

1. Identify the exact MOSFET suffix and confirm allowed **VGS** and **VDS**, with margin. Gate was observed near 23 V in TX tests; check VGS relative to SOURCE.
2. Use a scope probe rated for the observed drain peaks. Never attach an earth-referenced scope ground lead to DRAIN.
3. With all other circuitry disconnected, confirm the actual BAV99 node behavior, diode orientation, DC level near TX rail and switching excursions.
4. Keep CYD GPIO35 disconnected for every analog step below.

## 1. MCP602 supply only

From CYD 3.3 V, fit the planned **22 Ω + 22 µF** RC filter and local **100 nF** bypass at pins 8/4. Check 3.3 V, ripple and turn-on behavior.

## 2. Passive 1.65 V reference

Connect **10 kΩ / 10 kΩ** divider between filtered 3.3 V and GND with **10 µF + 100 nF** to GND at VREF; check the unloaded voltage and noise. Connect VREF to MCP602 A non-inverting pin 3. Do not leave the second amplifier inputs floating.

## 3. Design and test the AC-coupled gain stage

Determine C_COUPLE, R_IN, R_F, input clamps, feedback compensation and bias/return paths from oscilloscope measurements of the BAV99-protected node. Those component values are **not yet finalized**. A limiter around a ~23 V DC node does not make it directly safe for a 3.3 V op-amp.

Measure MCP602 output on the scope while pulsing TX. Verify short recovery from TX OFF, especially at the three new target sample times of **30, 60 and 90 µs**.

## 4. Validate ADC safety before wiring

Candidate output filter: **1 kΩ** series from MCP602 OUT to ADC node, **1 nF** from the node to ground (tentative). Check scope waveforms for overshoot/undershoot, output clipping, startup/shutdown behavior and power sequencing. Provide additional protection where needed. Only then connect to **GPIO35 / ADC1_CH7**.

## 5. First metal measurements

- Log no-target baseline, then a large steel/cast iron target at controlled distances.
- Compare early, middle and late decay with the scope; note possible coil detuning or probe artifacts.
- Once the analog output is safe, collect firmware raw taps and deltas, EARLY(30 µs)/MID(60 µs)/LATE(90 µs), score, persistence and late-sample counts.
- The existing M0 score is **diagnostic only**, not calibrated metal identification or detection depth.

## Parallel open issue: drain clamp

Design a repetitive pulse overvoltage solution for the 600 V nominal MOSFET with reserve for possible lower-resistance ~0.5 mm coil. The old SMBJ100A proposal is **not currently installed**, and present ~250 V peak cannot be treated as a guaranteed future maximum.

See [RX design worklist](RX_MCP602.md).

## SET-screen validation (live timing firmware)

Before applying TX power, check GPIO22 on a scope: select 25, 50, 75 and 100 µs pulse widths and confirm that the HIGH times change and the period remains 5 ms. Reconfiguration causes a brief intentional pause. Check 10/20/30 and 30/60/90 µs ADC settings via the startup/configuration log; watch LATE and MAX after enabling the receiver. With a stationary coil, select AVG 5 then AVG 10 to compare displayed noise against response latency. The new controls have not yet passed compilation or on-board verification.

## Timing rejection and nonvolatile settings (experimental)

- A sample is accepted only when the software ADC call is initiated within **0–1 µs after** the selected GPIO22 falling-edge-relative target. The ADC internal sample instant cannot be guaranteed; the `ADC` metric reports the worst call duration.
- A frame is valid only if **each** of the t/2t/3t windows has at least **8 accepted ADC calls** out of 16. Otherwise `ADC TIMING SKIP` appears and no score, moving average, or ZERO baseline is updated. `OK a/b/c`, `LATE`, `MAX`, and `ADC` quantify timing misses.
- With **10/20/30 µs** taps, if `ADC TIMING SKIP` appears consistently, first inspect `OK` counts and `ADC` runtime. A conversion lasting longer than the gap between taps cannot be made deterministic merely by filtering. Choose a later first tap until the acquisition mechanism is improved.
- Settings are committed automatically to **NVS** (namespace `cyd_metal`, one versioned `acq` blob): TX width, first ADC tap, moving-average window. Power-cycle to verify they persist. The stored record is read before RMT TX starts. NVS erase/flash erase resets settings.
- Flashing the new build and testing this timing/persistence revision are still pending.

## ADC one-sample-per-TX revision

Rigol/CYD debugging found the ADC oneshot call taking as much as **39 µs**, which made back-to-back taps (e.g. 10/20/30 µs) impossible. The firmware now acquires only **one tap after each TX falling edge**, in the sequence t → 2t → 3t over three successive hardware pulses; it repeats the sequence 16 times. Thus each tap has 16 opportunities, spread over **48 TX pulses ≈ 240 ms** per frame. RMT remains running at 200 Hz. Watch `OK` counts; on the previous failing setting, `16/16 0/16 0/16` should be replaced by valid values for all taps if scheduling jitter is sufficiently small. If invalid, inspect `MAX`/serial logs. With AVG 10, expect substantial response latency (~2.4 s rolling history). This change is still uncompiled/unverified on the CYD.

## AVG OFF test

SET page offers **AVG OFF / AVG 5 / AVG 10** along the bottom; use the top-right **BACK** control to return. AVG OFF is internally a one-frame window: the current 16 accepted samples per tap are still averaged, but frames are not smoothed together. It should respond faster (~240 ms per frame at 200 Hz) at the cost of more visible noise. The mode is stored in the existing NVS settings record and restored on reboot. Change modes with no metal near the coil because changing settings resets ZERO.
