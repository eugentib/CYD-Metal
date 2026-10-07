#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DETECTOR_TAP_COUNT 5

typedef esp_err_t (*detector_adc_read_fn)(void *ctx, int *raw);
typedef void (*detector_tx_set_fn)(void *ctx, bool high);

typedef struct {
    detector_tx_set_fn tx_set;
    detector_adc_read_fn adc_read;
    void *ctx;
} detector_io_t;

typedef struct {
    uint32_t pulse_width_us;
    uint32_t pulse_period_us;
    uint16_t tap_us[DETECTOR_TAP_COUNT];
    uint8_t pulses_per_frame;
} detector_config_t;

typedef struct {
    detector_config_t cfg;
    detector_io_t io;
    uint16_t baseline[DETECTOR_TAP_COUNT];
    bool baseline_valid;
} detector_t;

typedef struct {
    uint16_t raw[DETECTOR_TAP_COUNT];
    int16_t diff[DETECTOR_TAP_COUNT];

    uint16_t early;
    uint16_t mid;
    uint16_t late;
    uint16_t score;
    uint16_t persistence_pct;

    uint16_t late_samples;
    bool baseline_valid;
} detector_frame_t;

detector_config_t detector_default_config(void);

esp_err_t detector_init(detector_t *detector,
                        const detector_config_t *config,
                        const detector_io_t *io);

esp_err_t detector_capture_frame(detector_t *detector, detector_frame_t *frame);

void detector_zero_from_frame(detector_t *detector,
                              const detector_frame_t *frame);

#ifdef __cplusplus
}
#endif
