# Discrete BJT gate driver — experiment A

**Status: proposed prototype; not yet scope-tested.** This is a pin-level wiring reference for controlling an STP/STB13NK60Z N-channel power MOSFET from a classic ESP32 GPIO22 on a CYD 3.5 board.

Parts already available in the workshop:
- Q1 = BC847A NPN (SOT-23, marking 1E)
- Q2 = BC807 PNP (SOT-23, marking 5D*, e.g. 5DW)
- Q3 = BC847A NPN (SOT-23, marking 1E)
- D1 = **BAT54 single Schottky**, not BAT54A/C/S (those have different internal connection/pinning).
- M1 = STP/STB13NK60Z N-channel 600-V power MOSFET.

## Exact electrical netlist

All grounds below are common, except where specifically indicated as the MOSFET *source* node.

| Reference | Value / device | Connection A | Connection B |
|---|---|---|---|
| R1 | 1 kΩ | ESP32 GPIO22 | Q1 base |
| R2 | 47 kΩ | Q1 base | GND |
| Q1 | BC847A | emitter → GND | collector → **CTRL_LOW** |
| R3 | 1 kΩ | CTRL_LOW | Q2 base |
| R4 | 2.2 kΩ | Q2 base | +12 V |
| Q2 | BC807 | emitter → +12 V | collector → R5 |
| R5 | 100 Ω | Q2 collector | **MOS_GATE** |
| R6 | 4.7 kΩ | +12 V | Q3 base |
| D1 | BAT54 | **anode → Q3 base** | **cathode → CTRL_LOW** |
| Q3 | BC847A | emitter → GND | collector → R7 |
| R7 | 150 Ω | Q3 collector | MOS_GATE |
| R8 | 10 kΩ | MOS_GATE | **MOS_SOURCE** |
| M1 | STP/STB13NK60Z | gate → MOS_GATE | source → MOS_SOURCE |
| C1 | 100 nF ceramic | +12 V | GND |
| C2 | 10–47 µF electrolytic | +12 V | GND |

Q1 collector / CTRL_LOW connects **both** R3 and the BAT54 cathode. The R6/D1 junction connects the Q3 base. This is intentionally a discrete prototype, **not** a characterized dedicated gate-driver IC.

**BAT54 SOT-23 pinout** (Nexperia single BAT54):
- pin 1 = anode (to Q3 base)
- pin 2 = not connected
- pin 3 = cathode (to Q1 collector / CTRL_LOW)

**BC847A/BC807 SOT-23 pinout** (verify manufacturer and marking):
- pin 1 = base
- pin 2 = emitter
- pin 3 = collector

## Operation

| GPIO22 | Q1 | Q2 (charge gate) | Q3 (discharge gate) | MOSFET |
|---|---|---|---|---|
| LOW or high-Z | OFF | OFF | ON | OFF |
| HIGH | ON | ON | OFF | ON |

- R2 pulls GPIO/Q1's base down during ESP32 reset.
- Q1 pulls CTRL_LOW down when GPIO22 is HIGH.
- The BAT54 clamps Q3's base to near the low CTRL_LOW level, switching Q3 OFF while the gate is charged by Q2.
- When GPIO22 is LOW, Q2 releases MOS_GATE and Q3 discharges it through R7.
- A Schottky clamp is used rather than an ordinary silicon diode because an excessively high Q3 base voltage could cause simultaneous conduction of Q2/Q3.
- At approximately 12 V, Q1 sinks roughly 10–15 mA when commanded HIGH; BC847A should be within its 100 mA nominal collector-current rating.
- R7 limits Q3's initial gate-discharge peak to roughly 12 V/150 Ω = **80 mA** (before transistor saturation drops); short gate transients only. R5 similarly limits charging current.
- Storage-time overlap and actual turn-off dynamics are **not yet measured**. Check on a scope before using the circuit with an inductive load.

## Bring-up procedure — before connecting the coil

1. Assemble the gate driver with **no coil and no MOSFET drain supply circuit connected**. Include C1/C2 close to the BJTs.
2. Connect CYD and driver common GND. Supply the driver +12 V from a current-limited bench supply.
3. Probe **gate-to-source voltage VGS** using an oscilloscope, with its ground on the MOSFET source. CYD-Metal M0 outputs GPIO22 pulses with 100 µs HIGH every 5 ms.
4. Expected: near 0 V between pulses, about 11–12 V during HIGH; check leading and trailing edges.
5. Verify GPIO22 logic and a clean gate waveform. Check for overlap/current spikes, extended gate tail, overshoot and ringing. Keep wiring short.
6. Add the coil **only after implementing a proper drain clamp** and verifying its polarity, rating, flyback energy, and safe D-S peak voltage at the scope. First test at reduced/limited supply voltage.

## Coil and drain safety

**Do not connect the coil yet.** A 600 V MOSFET rating does not replace transient protection, and the BJT driver does not absorb the coil's stored inductive energy. A correctly rated unidirectional TVS drain-to-GND clamp (cathode at DRAIN, anode at GND) is one candidate; select the clamp with energy and expected pulse repetition in mind.

Do **not** attach GPIO35 to the drain. The ADC sees only a verified conditioned receiver output within 0–3.3 V.

The board's MOSFET shunt/source arrangement must be verified. If MOS_SOURCE sits above GND due to a shunt, measure VGS relative to **MOS_SOURCE**, not only relative to GND.

## Technical references

- Nexperia BAT54: https://assets.nexperia.com/documents/data-sheet/BAT54.pdf
- Nexperia BC807 series: https://assets.nexperia.com/documents/data-sheet/BC807_SER.pdf
- Nexperia BC847 series: https://assets.nexperia.com/documents/data-sheet/BC847_SER.pdf
- ST 13NK60Z MOSFET: https://www.st.com/resource/en/datasheet/stp13nk60z.pdf
