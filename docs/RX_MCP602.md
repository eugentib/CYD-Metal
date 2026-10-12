# MCP602 analog RX: planned build and validation

**Status 2026-10-13: design direction chosen; MCP602 signal chain NOT assembled or connected to the ADC.** The BAV99 input limiter is **already built and tested**. This is a work-in-progress wiring outline, **not yet a complete, validated schematic**.

## Signal path

```text
MOSFET DRAIN
     |
    15 kΩ              physically installed and tested
     |
RX_PROTECTED --- BAV99 limiter referenced to +V_TX
     |
     |                 not yet built:
     +--- C_COUPLE --- R_IN ---+---- MCP602 A inverting input (pin 2)
                               |
                              R_F
                               |
MCP602 A output (pin 1) -------+
      |
      +-- 1 kΩ (tentative) -- GPIO35 / ADC1_CH7
                                |
                               1 nF (tentative)
                                |
                               GND

MCP602 A non-inverting input (pin 3) -> VREF ~1.65 V
```

**Important:** the diagram is a functional signal chain. The node RX_PROTECTED can sit around +V_TX (~23 V at the tested supply), and the high-voltage switching event can impose fast excursions. **Do not connect it directly to MCP602.** AC coupling alone does not guarantee the amplifier input remains within absolute maximum ratings: coupling transient behavior, bias return and appropriate input clamps/protection still require a detailed design and scope verification.

The two BAV99 diodes were described in the bench notes as an antiparallel limiter to the TX supply. Confirm the exact diode orientation and node voltages on the assembled PCB before finalizing the next stage.

## Op-amp and power

Chosen part: **MCP602**, dual, rail-to-rail output, supply range approximately 2.7–5.5 V; gain-bandwidth ~2.8 MHz and slew rate ~2.3 V/µs. Planned supply **3.3 V** from the CYD; no negative rail.

```text
CYD 3.3 V --- 22 Ω --- +3V3_RX ----- MCP602 pin 8 (VDD)
                         |
                        22 µF
                         |
                        GND

Local 100 nF ceramic from MCP602 pin 8 to pin 4 (GND).

+3V3_RX --- 10 kΩ ---+--- 10 kΩ --- GND
                      |
                  VREF ~1.65 V
                      |
                 10 µF + 100 nF
                      |
                     GND
```

MCP602 A: pin 1 OUT A, pin 2 IN− A, pin 3 IN+ A. Pin 4 VSS/GND; B channel pin 5 IN+ B, pin 6 IN− B, pin 7 OUT B; pin 8 VDD.

VREF goes to pin 3. It is deliberately **passive/unbuffered** for the initial prototype. The effects of VREF source impedance, bypass-capacitor charge, return paths and signal-induced reference movement must still be measured. If channel B remains unused, configure it as a stable closed-loop stage rather than leaving inputs floating.

## First-stage transfer, values still open

In the valid linear region:

```text
VOUT ≈ VREF − (R_F / R_IN) × ΔV_RX
```

The **coupling capacitor**, **R_IN**, **R_F**, any **feedback capacitor** and protection/clamps are **not selected yet**. Do not fill these with arbitrary values or assume the amplifier will recover quickly after a 250 V drain transient.

The intent is to detect small decay differences in about **40–200 µs after TX OFF**, while avoiding long op-amp saturation, excessive settling time and ADC out-of-range events. The CYD software also samples at 300 µs, and performs 16-pulse averaging.

## Tentative ADC connection — do not install until measurements pass

```text
MCP602 OUT A -> 1 kΩ -> GPIO35
                         |
                        1 nF
                         |
                        GND
```

This is a provisional noise filter, **not a complete overvoltage protection network**. Validate signal bandwidth, sample timing, input current and ADC safety at startup/shutdown and during all switching transients. Protect GPIO35 against excursions outside its permitted input limits.

## Next bench steps

1. Assemble only the filtered 3.3 V supply and passive VREF; confirm supply, stability, startup and 1.65 V reference.
2. Determine C_COUPLE, input bias/return network, Rin/Rf, compensation and limiting components using the **measured** RX_PROTECTED waveform and MCP602 device limits.
3. Power and probe the first stage with a scope; inspect for rail clipping and actual settling after TX OFF.
4. Move a metal target at controlled distances and compare oscilloscope waveforms with/without target.
5. Verify safe 0–3.3 V-class GPIO35 levels through startup/shutdown and transients **before** connecting the CYD ADC.
6. Only after RX hardware tests, integrate GPIO35 and reconsider sample taps, timing method and gain.

## Parallel TX safety task

Independently design MOSFET drain protection for the currently observed ~250 V at 23 V TX **and** for any later lower-resistance coil. The RX limiter (BAV99 + 15 kΩ) is not a DRAIN clamp.
