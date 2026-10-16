#include "detector_core.h"

#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"

/*
 * Deterministic request-time acceptance policy. Only ADC calls started
 * within 1us of each intended tap are included in an average.
 * A full frame needs at least half of its 16 pulses valid at EVERY tap.
 * This cannot guarantee the internal sample-and-hold aperture.
 */
#define DETECTOR_ADC_MAX_LATENESS_US 1U
#define DETECTOR_MIN_VALID_DIVISOR 2U

static uint16_t u16_abs_i16(int16_t value)
{
    return (uint16_t)(value < 0 ? -value : value);
}

detector_config_t detector_default_config(void)
{
    detector_config_t cfg = {
        .pulse_width_us = 100,
        .pulse_period_us = 5000, /* 200 Hz PRF */
        .tap_us = {30, 60, 90},
        .pulses_per_frame = 16,
        .moving_avg_frames = 10,
    };
    return cfg;
}

esp_err_t detector_init(detector_t *detector,
                        const detector_config_t *config,
                        const detector_io_t *io)
{
    if (!detector || !config || !io ||
        !io->tx_wait_off || !io->adc_read ||
        config->pulses_per_frame == 0 ||
        (config->moving_avg_frames != 1 &&
         config->moving_avg_frames != 5 &&
         config->moving_avg_frames != 10) ||
        config->pulse_width_us == 0 ||
        config->pulse_width_us >= config->pulse_period_us) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->tap_us[0] == 0 ||
        config->pulse_width_us + config->tap_us[DETECTOR_TAP_COUNT - 1] >=
            config->pulse_period_us) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 1; i < DETECTOR_TAP_COUNT; ++i) {
        if (config->tap_us[i] <= config->tap_us[i - 1]) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    memset(detector, 0, sizeof(*detector));
    detector->cfg = *config;
    detector->io = *io;
    return ESP_OK;
}

esp_err_t detector_capture_frame(detector_t *detector, detector_frame_t *frame)
{
    if (!detector || !frame) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(frame, 0, sizeof(*frame));

    uint32_t sums[DETECTOR_TAP_COUNT] = {0};
    uint8_t counts[DETECTOR_TAP_COUNT] = {0};

    /*
     * RX capture strategy follows the available time between taps:
     *
     * first tap < 50 us: one ADC read per TX pulse, round-robin across
     * the three taps. The 39us worst observed adc_oneshot_read() runtime
     * cannot fit reliably between earlier windows.
     *
     * first tap >= 50 us: three ADC reads after ONE TX falling edge,
     * at t, 2t, 3t. Observed worst-case ADC call duration was ~39us;
     * a 50us gap leaves ~11us for return/dispatch overhead. If any
     * conversion starts later than target+1us, it is discarded exactly
     * as in interleaved mode (no unsafe assumption about timing).
     *
     * TX continues indefinitely in RMT in either mode. Each frame
     * averages 16 accepted readings for each tap, using 16 or 48
     * consecutive RMT pulses depending on capture mode.
     */
    const bool capture_three_per_pulse = detector->cfg.tap_us[0] >= 50U;

    for (uint8_t repetition = 0;
         repetition < detector->cfg.pulses_per_frame; ++repetition) {
        int64_t shared_rx_t0 = 0;
        if (capture_three_per_pulse) {
            esp_err_t err = detector->io.tx_wait_off(
                detector->io.ctx, &shared_rx_t0);
            if (err != ESP_OK) {
                return err;
            }
        }
        for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
            int64_t rx_t0 = shared_rx_t0;
            if (!capture_three_per_pulse) {
                esp_err_t err = detector->io.tx_wait_off(
                    detector->io.ctx, &rx_t0);
                if (err != ESP_OK) {
                    return err;
                }
            }

            const uint32_t target_us = detector->cfg.tap_us[i];

            while ((uint32_t)(esp_timer_get_time() - rx_t0) < target_us) {
                /* M0 intentionally favors timing visibility over CPU efficiency. */
            }

            /* Timestamp as close to the ADC request as possible.
             * There can still be preemption before/inside adc_read().
             * The diagnostic is request-start time, NOT true ADC aperture. */
            const int64_t adc_start = esp_timer_get_time();
            const uint32_t actual_us = (uint32_t)(adc_start - rx_t0);
            const uint32_t lateness =
                actual_us > target_us ? actual_us - target_us : 0U;
            if (lateness > frame->max_late_us) {
                frame->max_late_us = (uint16_t)(lateness > UINT16_MAX
                                                   ? UINT16_MAX : lateness);
            }
            if (lateness > DETECTOR_ADC_MAX_LATENESS_US) {
                /* Never contaminate the average with a late reading.
                 * The next pulse will sample the next scheduled tap. */
                frame->late_samples++;
                frame->rejected_samples[i]++;
                continue;
            }

            int raw = 0;
            esp_err_t err = detector->io.adc_read(detector->io.ctx, &raw);
            const uint32_t adc_elapsed_us =
                (uint32_t)(esp_timer_get_time() - adc_start);
            if (adc_elapsed_us > frame->max_adc_us) {
                frame->max_adc_us = (uint16_t)(adc_elapsed_us > UINT16_MAX
                                                   ? UINT16_MAX : adc_elapsed_us);
            }
            if (err != ESP_OK) {
                return err;
            }

            if (raw < 0) raw = 0;
            if (raw > 4095) raw = 4095;
            sums[i] += (uint32_t)raw;
            counts[i]++;
            frame->accepted_samples[i]++;
        }
    }

    /* A measurement with too few valid samples is not a measurement.
     * Skip the ENTIRE frame to keep the three tap averages comparable,
     * and never update the rolling history or ZERO from a partial frame.
     */
    const uint8_t required = (uint8_t)(
        (detector->cfg.pulses_per_frame + DETECTOR_MIN_VALID_DIVISOR - 1U) /
        DETECTOR_MIN_VALID_DIVISOR);
    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        if (counts[i] < required) {
            frame->frames_averaged = detector->history_count;
            return ESP_OK; /* data_valid remains false */
        }
    }
    frame->data_valid = true;

    /* No RMT stop or pause: display work occurs with TX still active.
     * Each frame averages accepted pulses (usually 16 for each tap).
     * window=1 disables cross-frame smoothing; 5/10 provide smoothing.
     */
    const uint8_t window = detector->cfg.moving_avg_frames;
    const uint8_t slot = detector->history_next;
    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        uint16_t sample = (uint16_t)(
            (sums[i] + counts[i] / 2U) / counts[i]);
        frame->instant_raw[i] = sample;
        if (detector->history_count == window) {
            detector->history_sum[i] -= detector->history[i][slot];
        }
        detector->history[i][slot] = sample;
        detector->history_sum[i] += sample;
    }
    if (detector->history_count < window) {
        detector->history_count++;
    }
    detector->history_next = (uint8_t)((slot + 1) % window);
    frame->frames_averaged = detector->history_count;
    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        frame->raw[i] = (uint16_t)(
            (detector->history_sum[i] + detector->history_count / 2U) /
            detector->history_count);
    }

    if (!detector->baseline_valid) {
        for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
            detector->baseline[i] = frame->raw[i];
        }
        detector->baseline_valid = true;
        frame->baseline_valid = false;
    } else {
        frame->baseline_valid = true;
    }

    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        int32_t d =
            (int32_t)frame->raw[i] - (int32_t)detector->baseline[i];
        if (d > INT16_MAX) d = INT16_MAX;
        if (d < INT16_MIN) d = INT16_MIN;
        frame->diff[i] = (int16_t)d;
    }

    /* Three independent post-TX-OFF measurements: t / 2t / 3t. */
    frame->early = u16_abs_i16(frame->diff[0]);
    frame->mid = u16_abs_i16(frame->diff[1]);
    frame->late = u16_abs_i16(frame->diff[2]);

    /*
     * Diagnostic weighting only. It intentionally favors the late response
     * because the first use-case is locating a large iron manhole cover.
     */
    frame->score = (uint16_t)(
        ((uint32_t)frame->early +
         2U * frame->mid +
         4U * frame->late) / 7U);

    uint32_t persistence =
        ((uint32_t)frame->late * 100U) /
        ((uint32_t)frame->early + 1U);
    if (persistence > 999U) {
        persistence = 999U;
    }
    frame->persistence_pct = (uint16_t)persistence;

    return ESP_OK;
}

void detector_zero_from_frame(detector_t *detector,
                              const detector_frame_t *frame)
{
    if (!detector || !frame || !frame->data_valid) {
        return; /* Never zero from a rejected/partial capture. */
    }

    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        detector->baseline[i] = frame->raw[i];
    }
    detector->baseline_valid = true;
}

esp_err_t detector_update_settings(detector_t *detector,
                                   uint32_t pulse_width_us,
                                   uint16_t first_tap_us,
                                   uint8_t moving_avg_frames)
{
    if (!detector || (pulse_width_us != 25 && pulse_width_us != 50 &&
                      pulse_width_us != 75 && pulse_width_us != 100) ||
        first_tap_us < 10 || first_tap_us > 100 ||
        (moving_avg_frames != 1 &&
         moving_avg_frames != 5 &&
         moving_avg_frames != 10)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (pulse_width_us + (uint32_t)first_tap_us * 3U >=
        detector->cfg.pulse_period_us) {
        return ESP_ERR_INVALID_ARG;
    }

    detector->cfg.pulse_width_us = pulse_width_us;
    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        detector->cfg.tap_us[i] = (uint16_t)(first_tap_us * (i + 1));
    }
    detector->cfg.moving_avg_frames = moving_avg_frames;

    /* Samples captured at different times/pulse widths are not comparable.
     * Reset both smoothed history and baseline on a setting change. */
    memset(detector->history, 0, sizeof(detector->history));
    memset(detector->history_sum, 0, sizeof(detector->history_sum));
    detector->history_count = 0;
    detector->history_next = 0;
    detector->baseline_valid = false;
    return ESP_OK;
}
