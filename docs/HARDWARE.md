# Hardware status and measurement log

**Updated: 2026-10-13.** This file describes the **physically assembled bench prototype**. Planned circuits are called out separately. The hardware concept is inspired by Spirit PI; no Spirit PI application firmware has been reused.

## Target and controller

Target: a large cast-iron/steel manhole cover hidden beneath concrete, reported to contain no reinforcing steel or mesh. Nearby electrical cables may be present. The goal is a portable detector powered from a nominal 18 V Makita battery.

CYD 3.5" / classic ESP32, ST7796 480×320, working touchscreen:

| Function | GPIO | Notes |
|---|---:|---|
| TX pulse | 22 | P3 |
| RX ADC | 35 / ADC1_CH7 | Input-only, **currently not connected to RX** |
| Speaker amplifier | 26 | Onboard |
| RGB R/G/B | 4 / 16 / 17 | Onboard, active-low |
| BOOT / ZERO | 0 | Do not hold through boot unless intentionally flashing |
| TFT backlight | 27 | Used by display |

Firmware configuration: ESP-IDF 5.4; **RMT hardware-clocked** 100 µs TX HIGH and 4,900 µs LOW, giving a stable 5 ms cycle (200 Hz) within 16-pulse capture bursts. ADC polling targets 40, 80, 130, 200 and 300 µs after GPIO22 falling-edge readback. This replaces the software delay that produced measured 100.45–104.39 µs pulse jitter. Display/touch updates remain outside capture. **The new RMT build has not yet been validated on the physical CYD.**

## Assembled coil and measurements

| Parameter | Current coil |
|---|---:|
| Plastic air-core former diameter | 210 mm |
| Turns | 30 |
| Enamelled wire diameter (estimated) | ~0.19 mm |
| Measured DC resistance | 12.4 Ω |
| Inductance (derived, not LCR-meter measured) | ~364 µH |
| Resonance with external ~3.5 nF | 139 kHz |
| Self-resonance, x10 scope probe | ~824 kHz |
| Effective stray capacitance (derived, includes probe) | ~103 pF |

External 3.5 nF test capacitor has been **removed**. Coil wires are a twisted pair.

A possible future coil uses ~0.5 mm wire with the same nominal geometry and turns. A resistance around 1.8 Ω is **only an estimate**, not a measured value. This would increase peak current/energy and requires renewed MOSFET protection analysis.

## Transmitter: physically assembled and scope-tested

```text
                       +V_TX
                         |
               +---------+---------+
               |                   |
         210 mm air coil         150 Ω
        ~364 µH / 12.4 Ω          |
               |                   |
               +---------+---------+---- DRAIN
                                          |
                                       13NK60
                                          |
                                        SOURCE
                                          |
                                         GND

GPIO22 -> discrete BC847A/BC807/BC847A + BAT54 driver -> GATE
```

- 600 V N-MOSFET from the B13NK60/13NK60 family; the exact suffix and its VGS maximum must be confirmed against the fitted device.
- GPIO22 HIGH = TX ON; GPIO22 LOW or high impedance = TX OFF, as tested.
- Driver gate supply **currently follows V_TX**; gate voltage up to ~23 V observed. Verify actual VGS transients and allowable VGS for the installed part.
- Observed resistive-load DRAIN transitions: roughly 300–360 ns. With the coil, switching works and strong post-off ringing was not observed.
- The present **150 Ω resistor is across the coil, between +V_TX and DRAIN**. The earlier 75 Ω resistor was replaced.
- No additional Zener/TVS DRAIN clamp is physically installed.

### Scope measurements

| Test condition | DRAIN peak |
|---|---:|
| V_TX 23 V, 75 Ω coil damping, previous configuration | ~145 V |
| V_TX 23 V, **150 Ω coil damping, current configuration** | **~250 V** |

The 250 V result is a **measured oscilloscope peak**, not the predicted limit for different coils or operating conditions. Larger current with a rewound coil can increase the peak. The 600 V VDS rating is an absolute device rating, **not a valid design clamp voltage**. A suitably rated clamp must be chosen by expected worst-case voltage, pulse energy, repetitive power at 200 Hz, and failure scenarios such as a disconnected damping resistor. Avoid clamping normal decay unnecessarily, but retain margin below device limits.

See [discrete gate driver and TX documentation](DISCRETE_BJT_DRIVER.md).

## Receiver input: BAV99 assembled, MCP602 in bench testing

The physical input network has been built and tested:

```text
DRAIN ── 15 kΩ ── RX_PROTECTED ── AC coupling ── 47 kΩ ── MCP602 IN−
                     |
                 BAV99 limiter
                     |
                    +V_TX
```

The BAV99 arrangement is described in the bench record as a two-diode antiparallel limiting network referenced to +V_TX. With V_TX ≈ 23 V the protected node sits near the TX supply, **not at ground**. Before attaching another circuit, confirm diode orientation and voltages on the actual build; the schematic above is a functional summary rather than a package-pin netlist.

**The BAV99 network is RX-side protection only; it does not protect the MOSFET from DRAIN transients.**

### MCP602 receiver: bench assembly and measurements

- Dual MCP602, nominal 2.7–5.5 V supply; **3.3 V** selected from filtered CYD rail.
- Proposed rail filter: CYD 3.3 V → 22 Ω → MCP602 VDD, 22 µF to GND; 100 nF local VDD-to-GND bypass.
- Passive VREF: 10 kΩ from filtered 3.3 V to VREF; 10 kΩ VREF-to-GND; 10 µF + 100 nF VREF-to-GND. Nominal VREF ≈ 1.65 V.
- First amplifier is being tested as an AC-coupled **inverting** stage, non-inverting input at VREF. Initial measured setup: **Rin = 47 kΩ, Rf = 180 kΩ, Ccoupling = 85 nF** (previously 3.5 nF), with antiparallel feedback diodes to limit TX transients. Approximate small-signal linear response: `VOUT = VREF - (Rf/Rin) · ΔVRX`, but the diode limiter makes the large-signal gain nonlinear.
- With the earlier 3.5 nF capacitor, output saturation/recovery after TX OFF was observed; feedback diodes improved behavior. Actual metal response is not yet characterized.
- Second op-amp available for later filtering/gain; if unused, terminate it in a stable valid configuration.
- Preliminary ADC path: MCP602 OUT → **1 kΩ series** → GPIO35, with **1 nF to GND at the ADC node**. Not yet approved for connection.

MCP602 DIP/SOIC-8 pin functions: 1 OUT A, 2 IN− A, 3 IN+ A, 4 VSS, 5 IN+ B, 6 IN− B, 7 OUT B, 8 VDD.

See [MCP602 RX plan](RX_MCP602.md) for the checklist. Guard against amplifier saturation immediately after TX OFF, especially in the desired ~40–200 µs observation interval.

## Power plan (not a finished power schematic)

| Block | Planned supply |
|---|---|
| TX coil / MOSFET power circuit | Makita nominal 18 V battery (~21 V fully charged) |
| Gate driver | TX rail, provided actual safe VGS is confirmed |
| CYD | Battery-fed DC/DC to 5 V |
| MCP602 | Filtered CYD 3.3 V |
| MCP602 signal reference | Passive 1.65 V divider |

Common signal ground required. Keep high-current pulse return out of sensitive RX ground routes. Include battery fuse and undervoltage protection. Do not attach earth-referenced scope ground to DRAIN or the coil's positive rail.

## Current observation and open questions

Approaching a metal cabinet within ~1–2 cm produced visible changes in the scope decay curve, including ~150–200 µs after TX OFF. It is **not yet demonstrated** whether the change is a true eddy-current target response, coil detuning or a measurement artifact. The analog RX chain must be measured before claiming sensitivity or depth.
