# Roadmap

## M0 - timing and analog bring-up

- [x] ESP-IDF project scaffold
- [x] CYD 3.5 board profile
- [x] TX pulse generation
- [x] ADC1 / GPIO35 sampling
- [x] five decay taps
- [x] pulse averaging
- [x] ZERO baseline
- [x] EARLY/MID/LATE diagnostics
- [x] RGB/audio feedback
- [ ] verify on real CYD + Spirit PI front-end

## M1 - acquisition engine

- [ ] replace oneshot sampling with ADC continuous/DMA
- [ ] capture full decay waveform
- [ ] hardware-timed TX scheduling
- [ ] timing/lateness telemetry
- [ ] configurable pulse width and PRF
- [ ] configurable blanking/sample window
- [ ] raw waveform serial export

## M2 - CYD UI

- [x] ST7796 display driver
- [x] resistive/capacitive touch board profile
- [x] basic large target meter
- [ ] live decay graph
- [ ] baseline overlay
- [ ] EARLY/MID/LATE bars
- [x] ZERO button on screen
- [ ] sensitivity and pulse settings
- [x] basic mute control

## M3 - manhole-cover mode

- [ ] characterize reinforcing-bar response
- [ ] characterize large cast-iron/steel cover response
- [ ] persistence / decay-shape features
- [ ] scan mode
- [ ] optional spatial heat map
- [ ] large-coil experiments

## M4 - headless build

- [ ] ESP32 board profile
- [ ] NeoPixel feedback
- [ ] speaker/buzzer feedback
- [ ] physical ZERO button
- [ ] same detector_core without display dependencies
