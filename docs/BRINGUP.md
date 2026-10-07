# M0 bring-up

The goal of M0 is not maximum detection depth. It is to establish clean, repeatable timing and record how the analog front-end behaves.

## 1. CYD only

Flash with the detector hardware disconnected.

Expected:

- serial log starts;
- RGB LED is green;
- no reset loop;
- speaker is quiet at low score.

The firmware auto-captures its first baseline frame. With the ADC floating this has no physical meaning; this step only checks firmware stability.

## 2. TX output

Connect detector ground to CYD ground.

Connect GPIO22 to the detector pulse-input node.

With an oscilloscope on GPIO22, expect approximately:

- pulse width: 100 us;
- repetition period: 5 ms;
- PRF: 200 Hz.

Do not continue until the pulse is clean and the detector MOSFET gate/drain behavior is understood.

## 3. Receiver output

Keep GPIO35 disconnected.

Probe the detector's conditioned analog output. Check that it is safe for the ESP32 ADC under all expected conditions.

Then connect:

```text
detector analog out -> CYD GPIO35
detector GND        -> CYD GND
```

## 4. First real baseline

Keep metal away from the coil and press BOOT briefly.

Serial should report:

```text
ZERO captured
```

The following frames should have small `d=[...]` values and a low score.

## 5. Large-target test

Move a steel plate, pan, manhole-like object or other large ferrous target toward the coil.

Record:

- raw taps;
- differential taps;
- EARLY;
- MID;
- LATE;
- score;
- persistence;
- late sample count.

The current taps are measured nominally at:

```text
40 us, 80 us, 130 us, 200 us, 300 us
```

after TX turns off.

## 6. Timing quality

`late=N` counts samples that started more than 15 us after their requested tap time.

A few late samples in M0 are useful information. A high count means the oneshot ADC/API overhead is limiting us and confirms that we should move to ADC continuous/DMA capture.

## What to send back after the first test

Useful data for the next iteration:

1. exact CYD model marking (for example ESP32-3248S035R or C);
2. oscilloscope screenshot of GPIO22 pulse;
3. oscilloscope screenshot of the conditioned analog output after TX-off;
4. 10-20 serial lines with no target;
5. 10-20 serial lines with a large steel target at a known distance.

From that we can select better sample times and implement the DMA acquisition engine.
