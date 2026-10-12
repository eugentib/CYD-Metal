# Discrete BJT gate driver and TX stage

**Updated 2026-10-13. Status: assembled and tested with an oscilloscope.** This replaces earlier notes calling this stage “proposed / not yet scope-tested”. See [actual hardware measurements](HARDWARE.md).

## Components and reference netlist

Parts used in the prototype:

- Q1: BC847A NPN (SOT-23 mark `1E`)
- Q2: BC807 PNP (SOT-23 mark `5DW`)
- Q3: BC847A NPN (SOT-23 mark `1E`)
- D1: BAT54 **single** Schottky
- M1: 13NK60-family N-channel MOSFET, nominal 600 V (verify suffix and VGS rating of the **fitted part**)

The following is the **previously published driver netlist**. The driver topology is tested; the bench summary does **not independently re-verify every individual resistor value** in this table. Compare to the populated board before using it as an as-built BOM.

| Ref | Documented value | From | To |
|---|---|---|---|
| R1 | 1 kΩ | GPIO22 | Q1 base |
| R2 | 47 kΩ | Q1 base | GND |
| Q1 | BC847A | emitter GND | collector CTRL_LOW |
| R3 | 1 kΩ | CTRL_LOW | Q2 base |
| R4 | 2.2 kΩ | Q2 base | V_DRIVER |
| Q2 | BC807 | emitter V_DRIVER | collector R5 |
| R5 | 100 Ω | Q2 collector | MOS_GATE |
| R6 | 4.7 kΩ | V_DRIVER | Q3 base |
| D1 | BAT54 | anode Q3 base | cathode CTRL_LOW |
| Q3 | BC847A | emitter GND | collector R7 |
| R7 | 150 Ω | Q3 collector | MOS_GATE |
| R8 | 10 kΩ | MOS_GATE | MOS_SOURCE |
| M1 | 13NK60 family | gate MOS_GATE | source MOS_SOURCE |
| C1 | 100 nF | V_DRIVER | GND |
| C2 | 10–47 µF | V_DRIVER | GND |

**R7 = 150 Ω is a gate discharge resistor; it is not the separate 150 Ω coil damping resistor.** The existing netlist originally labels the driver rail +12 V. On the tested assembly, **V_DRIVER follows V_TX**, reaching approximately 23 V, and is **not a regulated 12 V rail**.

Single BAT54 SOT-23 pinout (confirm manufacturer): 1 anode, 2 NC, 3 cathode. BC847A/BC807 SOT-23 typically: 1 base, 2 emitter, 3 collector. Verify actual package drawings before soldering.

## Tested logic and switching

| GPIO22 state | Driver effect | MOSFET |
|---|---|---|
| HIGH | Gate driven high | ON |
| LOW | Gate actively discharged | OFF |
| High impedance / boot | Pull-down holds safe default | OFF |

Q1 drives the complementary gate-charge / gate-discharge control. BAT54 helps turn Q3 off while Q2 charges the gate. Oscilloscope tests confirmed gate and DRAIN operation. Resistive-load DRAIN transitions were approximately **300–360 ns**; the coil switching was also observed without strong uncontrolled post-OFF oscillations.

The recorded gate voltage reached roughly **23 V** at the highest tested TX voltage. A maximum VGS of ±30 V was discussed for a particular device variant; **do not assume that limit for every 13NK60-family part**. Measure gate **relative to SOURCE**, including spikes and ringing, and check the installed MOSFET datasheet. Source and oscilloscope ground points must be chosen safely.

## Actual power stage (150 Ω, not 75 Ω)

```text
                +V_TX  (23 V in reported scope tests)
                     |
              +------+------+
              |             |
           L1 coil        150 Ω damping
        ~364 µH / 12.4 Ω     |
              |             |
              +------+------+
                     |
                   DRAIN
                     |
                M1 13NK60
                     |
                   SOURCE
                     |
                    GND
```

The coil is an air-core 210 mm, 30-turn winding of ~0.19 mm enamelled copper wire. TX ON ~100 µs, PRF 200 Hz.

| R across L1 | V_TX | Observed DRAIN peak | Status |
|---|---:|---:|---|
| 75 Ω | 23 V | ~145 V | **Previous** experiment |
| **150 Ω** | **23 V** | **~250 V** | **Currently fitted** |

No Zener or TVS clamp is installed across DRAIN-to-GND at this stage. In particular, the **SMBJ100A shown in the older proposal was not actually fitted**. The older “fit 75 Ω and SMBJ100A before connecting the coil” instruction is historical and does not describe the current validated bench assembly.

A 600 V MOSFET is not adequately protected merely because the present measured peak is ~250 V. A future ~0.5 mm winding with lower resistance could raise current and stored energy significantly; a purpose-designed repetitive overvoltage clamp is still a priority. Dimension the device for actual worst-case VDS, energy and pulse repetition, preserving margin and allowing the useful RX decay to remain visible.

## Receiver boundary

Physically assembled RX limiter: **DRAIN → 15 kΩ → BAV99 protected node referenced to +V_TX**; tested separately. This does **not** replace the MOSFET's own overvoltage clamp.

No connection should be made from DRAIN or the protected ~V_TX node directly to ESP32 GPIO35. The MCP602 AC-coupled receiver is still under development; see [RX_MCP602.md](RX_MCP602.md).

## Bench measurement precautions

1. Use a suitable high-voltage rated probe (and correct reference ground) for DRAIN. Do not connect the scope ground lead to DRAIN.
2. Measure actual VGS between GATE and SOURCE, not just gate relative to an arbitrary ground.
3. Check the worst pulse at maximum operating voltage, including power cycling, coil disconnection and any later coil redesign.
4. Keep GPIO35 disconnected until the completed RX output has been characterized under all transients.

## References

- [Nexperia BAT54](https://assets.nexperia.com/documents/data-sheet/BAT54.pdf)
- [Nexperia BC807](https://assets.nexperia.com/documents/data-sheet/BC807_SER.pdf)
- [Nexperia BC847](https://assets.nexperia.com/documents/data-sheet/BC847_SER.pdf)
- [ST 13NK60Z family datasheet](https://www.st.com/resource/en/datasheet/stp13nk60z.pdf) — only if this is the actual fitted suffix
