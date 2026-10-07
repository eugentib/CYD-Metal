#include "detector_core.h"

#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_rom_sys.h"

static uint16_t u16_abs_i16(int16_t value)
{
    return (uint16_t)(value < 0 ? -value : value);
}

detector_config_t detector_default_config(void)
{
    detector_config_t cfg = {
        .pulse_width_us = 100,
        .pulse_period_us = 5000, /* 200 Hz PRF */
        .tap_us = {40, 80, 130, 200, 300},
        .pulses_per_frame = 16,
    };
    return cfg;
}

esp_err_t detector_init(detector_t *detector,
                        const detector_config_t *config,
                        const detector_io_t *io)
{
    if (!detector || !config || !io ||
        !io->tx_set || !io->adc_read ||
        config->pulses_per_frame == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->tap_us[0] == 0 ||
        config->tap_us[DETECTOR_TAP_COUNT - 1] >= config->pulse_period_us) {
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

    for (uint8_t p = 0; p < detector->cfg.pulses_per_frame; ++p) {
        const int64_t cycle_start = esp_timer_get_time();

        detector->io.tx_set(detector->io.ctx, true);
        esp_rom_delay_us(detector->cfg.pulse_width_us);
        detector->io.tx_set(detector->io.ctx, false);

        const int64_t rx_t0 = esp_timer_get_time();

        for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
            const uint32_t target_us = detector->cfg.tap_us[i];

            while ((uint32_t)(esp_timer_get_time() - rx_t0) < target_us) {
                /* M0 intentionally favors timing visibility over CPU efficiency. */
            }

            const uint32_t actual_us =
                (uint32_t)(esp_timer_get_time() - rx_t0);
            if (actual_us > target_us + 15U) {
                frame->late_samples++;
            }

            int raw = 0;
            esp_err_t err = detector->io.adc_read(detector->io.ctx, &raw);
            if (err != ESP_OK) {
                detector->io.tx_set(detector->io.ctx, false);
                return err;
            }

            if (raw < 0) raw = 0;
            if (raw > 4095) raw = 4095;
            sums[i] += (uint32_t)raw;
        }

        const int64_t elapsed = esp_timer_get_time() - cycle_start;
        if (elapsed < detector->cfg.pulse_period_us) {
            esp_rom_delay_us(
                (uint32_t)(detector->cfg.pulse_period_us - elapsed));
        }
    }

    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        frame->raw[i] =
            (uint16_t)(sums[i] / detector->cfg.pulses_per_frame);
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

    const uint16_t a0 = u16_abs_i16(frame->diff[0]);
    const uint16_t a1 = u16_abs_i16(frame->diff[1]);
    const uint16_t a2 = u16_abs_i16(frame->diff[2]);
    const uint16_t a3 = u16_abs_i16(frame->diff[3]);
    const uint16_t a4 = u16_abs_i16(frame->diff[4]);

    frame->early = (uint16_t)((a0 + a1) / 2U);
    frame->mid = a2;
    frame->late = (uint16_t)((a3 + a4) / 2U);

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
    if (!detector || !frame) {
        return;
    }

    for (int i = 0; i < DETECTOR_TAP_COUNT; ++i) {
        detector->baseline[i] = frame->raw[i];
    }
    detector->baseline_valid = true;
}
