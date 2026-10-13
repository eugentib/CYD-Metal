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
