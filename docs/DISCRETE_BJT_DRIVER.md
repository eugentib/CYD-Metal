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


## Complete TX low-side power stage — for 210-mm coil

Use this in addition to the BJT-driver netlist above. **Prototype, not hardware-validated yet.**

```text
                   +12 V TX
                      |
             +--------+--------+
             |                 |
         L1 air coil         R9 = 75 ohm, >=1 W
       ~364 uH / 12.4 ohm      |
             |                 |
             +--------+--------+--------> DRAIN of M1 (pin 2)
                      |                   M1: STP13NK60Z / STB13NK60Z
          D2 SMBJ100A |                   GATE (pin 1) <- MOS_GATE
           uni TVS    |                   SOURCE (pin 3) -> GND
         cathode here |
           anode      |
               |      |
              GND     |
```

Note D2 is connected from **DRAIN to GND** with **cathode at DRAIN and anode at GND**, and is NOT in series with the coil. R9 is across the **coil terminals**, not across MOSFET D-S.

Use one 100-nF ceramic and a 47–220-uF electrolytic across the +12-V TX rail and GND near the coil/MOSFET wiring; put a separate 100-nF plus 10–47-uF bypass close to the BJT driver.

The TX supply and the gate-driver supply can come from the same current-limited 12-V source for early tests; CYD uses its normal 5-V supply and shares common GND. Keep the receiver ADC unconnected until a safe analog front-end exists.

### Nominal 12-V estimates at 100-us pulses, 200 Hz

- L1: 364 uH, Rcoil=12.4 ohm, Rds(on) roughly 0.5 ohm.
- Icoil near 0.93–0.94 A at end of pulse.
- R9 current during ON: 12/75 ~= 0.16 A; **total** MOSFET peak ~1.1 A.
- On turning OFF, coil-generated voltage across 75-ohm shunt ~0.93*75=70 V; drain voltage ~12+70=**82 V** (ideal, neglecting parasitics/overshoot).
- Coil ringdown time constant ~364 uH/(12.4+75) ~= 4.2 us; practically decays over ~20 us.
- R9 dissipates ~0.04 W average during the 2% TX-on period, plus roughly 0.03 W average if nearly all magnetic pulse energy dissipates in R9 (some also dissipates in coil copper). **Use a pulse-capable 1 W part** and verify its temperature.
- D2: **unidirectional SMBJ100A** has 100-V reverse standoff, breakdown >=111 V and specified clamp around 162 V at its rated pulse current. Its main role is protecting against spikes outside nominal shunt-controlled current decay, not clamping the expected ~82-V spike. Obtain/check genuine TVS specs before assembly.

### Physical connections / pinout

- STP13NK60Z in TO-220: pin 1=GATE, pin 2=DRAIN, pin 3=SOURCE; exposed tab=DRAIN. View the manufacturer's package drawing, not a mirrored back view. The STB D2PAK package also has 1=G, 2=D, 3=S, tab=D.
- BC847A and BC807 SOT-23: pin 1=B, pin 2=E, pin 3=C, top view pin 3 alone, pins 1 & 2 side-by-side.
- Single BAT54 SOT-23: pin 1=anode, pin 2=NC, pin 3=cathode. BAT54A/C/S contain **two diodes** with different pinouts and cannot be swapped pin-for-pin.

### Bench test checkpoints

1. With **no coil attached**, turn on the 12-V current-limited gate-driver supply and measure MOSFET VGS with GPIO22 pulses. It must show near 0 V in rest and ~11–12 V during 100-us TX ON. Watch the turn-off edge closely.
2. Then fit **both the 75-ohm shunt and the TVS before attaching the coil**.
3. Supply the TX rail at a reduced voltage first, e.g. 3–6 V, with a current limit high enough not to trip on the designed pulses. If shared with driver, the gate-driver supply must remain ~12 V; use two supplies or verify reliable operation at reduced gate bias.
4. Use a properly rated **x10 or high-voltage** oscilloscope probe from DRAIN to common SOURCE/GND; never attach the oscilloscope ground clip to the drain or the high side of the coil.
5. Verify VGS (CH1) and VDS (CH2), spikes, transistor temperature and MOSFET reset. Increase TX supply toward 12 V only after validating the waveform. **Do not connect GPIO35 ADC to the drain or coil.**
6. The long-term receiver input is a separate protected/conditioned output, limited to ESP32-safe voltages.

These values are the **initial TX schematic**. Receiver analog gain/protection and field testing are separate unfinished design work.

Technical source for TVS: https://www.diodes.com/part/view/SMBJ100A
