#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DETECTOR_TAP_COUNT 3
#define DETECTOR_MAX_AVG_FRAMES 10

typedef esp_err_t (*detector_adc_read_fn)(void *ctx, int *raw);
typedef esp_err_t (*detector_tx_wait_off_fn)(void *ctx, int64_t *tx_off_us);

typedef struct {
    detector_tx_wait_off_fn tx_wait_off;
    detector_adc_read_fn adc_read;
    void *ctx;
} detector_io_t;

typedef struct {
    uint32_t pulse_width_us;
    uint32_t pulse_period_us;
    uint16_t tap_us[DETECTOR_TAP_COUNT];
    uint8_t pulses_per_frame;
    uint8_t moving_avg_frames;  /* 5 or 10 frames */
} detector_config_t;

typedef struct {
    detector_config_t cfg;
    detector_io_t io;
    uint16_t baseline[DETECTOR_TAP_COUNT];
    uint16_t history[DETECTOR_TAP_COUNT][DETECTOR_MAX_AVG_FRAMES];
    uint32_t history_sum[DETECTOR_TAP_COUNT];
    uint8_t history_count;
    uint8_t history_next;
    bool baseline_valid;
} detector_t;

typedef struct {
    uint16_t raw[DETECTOR_TAP_COUNT];       /* rolling average */
    uint16_t instant_raw[DETECTOR_TAP_COUNT]; /* current 16-pulse frame */
    int16_t diff[DETECTOR_TAP_COUNT];

    uint16_t early;
    uint16_t mid;
    uint16_t late;
    uint16_t score;
    uint16_t persistence_pct;

    uint16_t late_samples;         /* acquisition requests >3us late */
    uint16_t max_late_us;          /* maximum ADC request-start lateness */
    uint8_t frames_averaged;      /* moving window currently populated */
    bool baseline_valid;
} detector_frame_t;

detector_config_t detector_default_config(void);

esp_err_t detector_init(detector_t *detector,
                        const detector_config_t *config,
                        const detector_io_t *io);

esp_err_t detector_capture_frame(detector_t *detector, detector_frame_t *frame);

/* Called only between captures; resets the rolling window and baseline. */
esp_err_t detector_update_settings(detector_t *detector,
                                   uint32_t pulse_width_us,
                                   uint16_t first_tap_us,
                                   uint8_t moving_avg_frames);

void detector_zero_from_frame(detector_t *detector,
                              const detector_frame_t *frame);

#ifdef __cplusplus
}
#endif
