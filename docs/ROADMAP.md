# Roadmap and verified status

**Updated: 2026-10-13.** Software bring-up and the existing TX hardware have progressed further than the original M0 checklist. **RX analog acquisition has not yet been field-validated**.

## M0 — firmware, display and TX hardware

- [x] ESP-IDF project scaffold, classic ESP32 and CYD 3.5 profile
- [x] ST7796 working UI, touchscreen and screen controls
- [x] TX pulse output on GPIO22: 100 µs / 200 Hz
- [x] Software ADC sampling framework, three taps (30/60/90 µs), 16-pulse averaging
- [x] ZERO/AUTOZERO, EARLY/MID/LATE, diagnostic score and logs
- [x] RGB/audio feedback
- [x] Discrete BJT gate driver assembled and observed with scope
- [x] 13NK60 TX low-side stage and 210 mm coil assembled and tested
- [x] 150 Ω damping fitted; ~250 V DRAIN peak measured at 23 V
- [x] 15 kΩ + BAV99 RX input limiter assembled/tested
- [ ] MCP602 RX amplifier assembled and validated
- [ ] Safe conditioned RX connected to GPIO35
- [ ] First reproducible metal-detection tests through the ADC

## M1 — analog safety and acquisition

- [ ] Design and validate MOSFET DRAIN clamp for present and future coil
- [ ] Complete MCP602 AC-coupled inverting gain, VREF and input protection
- [ ] Measure RX settling/saturation, noise and target delta at 30–90 µs
- [ ] Validate ADC protection, including startup/shutdown transients
- [ ] Compare nominal ADC sample times to actual receiver response
- [ ] Replace software oneshot sampling with improved hardware timing / ADC continuous-DMA if required
- [ ] Adjustable pulse width, pulse repetition and sample window
- [ ] Full waveform / raw data export and timing telemetry

## M2 — CYD UI refinement

- [x] ST7796 display driver and touch
- [x] Basic large target meter and diagnostic values
- [x] ZERO, MUTE and RAW/PERSIST controls
- [ ] Live decay graph and baseline overlay
- [ ] EARLY/MID/LATE response bars
- [ ] Sensitivity and TX-pulse configuration
- [ ] Calibrated readings based on real receiver measurements

## M3 — buried manhole-cover trials

- [ ] Identify true eddy-current response vs coil detuning / probe artifacts
- [ ] Controlled-distance experiments on large iron/steel targets
- [ ] Survey test location (reported as concrete without rebar or mesh)
- [ ] Evaluate nearby cable/metal interference
- [ ] Try ~0.5 mm wire coil and reassess energy, drain clamp and sensitivity
- [ ] Scan mode / optional spatial heat map

## M4 — portable/headless variants

- [ ] Makita 18 V battery input with fuse, undervoltage protection and DC/DC 5 V
- [ ] Better isolation of high-current return from analog ground
- [ ] Optional headless ESP32 + NeoPixel + speaker profile
- [ ] Durable case, coil support and field operation
