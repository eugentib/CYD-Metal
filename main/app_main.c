#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board_cyd35.h"
#include "detector_core.h"
#include "display_st7796.h"
#include "touch_cyd35.h"
#include "ui_cyd35.h"

static const char *TAG = "CYD-Metal";

static esp_err_t detector_tx_wait_off_adapter(void *ctx, int64_t *tx_off_us)
{
    (void)ctx;
    return board_cyd35_tx_wait_off(tx_off_us);
}

static esp_err_t detector_adc_adapter(void *ctx, int *raw)
{
    (void)ctx;
    return board_cyd35_adc_read(raw);
}

void app_main(void)
{
    ESP_ERROR_CHECK(board_cyd35_init());

    ESP_LOGI(TAG, "Initializing ST7796 display...");
    ESP_ERROR_CHECK(display_st7796_init());

    ESP_LOGI(TAG, "Detecting touchscreen...");
    ESP_ERROR_CHECK(touch_cyd35_init());
    ESP_LOGI(TAG, "Touch controller: %s", touch_cyd35_type_name());

    ESP_ERROR_CHECK(ui_cyd35_init(touch_cyd35_type_name()));
    ESP_LOGI(TAG, "Live detector UI ready");

    detector_config_t cfg = detector_default_config();
    detector_io_t io = {
        .tx_wait_off = detector_tx_wait_off_adapter,
        .adc_read = detector_adc_adapter,
        .ctx = NULL,
    };
    detector_t detector;

    ESP_ERROR_CHECK(detector_init(&detector, &cfg, &io));

    ESP_LOGI(TAG, "CYD-Metal M0 starting (continuous RMT hardware TX)");
    ESP_LOGI(TAG, "TX GPIO=%d, ADC GPIO=%d / ADC1_CH7",
             BOARD_CYD35_TX_GPIO, BOARD_CYD35_ADC_GPIO);
    ESP_LOGI(TAG, "BOOT or on-screen ZERO captures the baseline");
    ESP_LOGI(TAG, "ADC taps after TX OFF: %u / %u / %u us",
             cfg.tap_us[0], cfg.tap_us[1], cfg.tap_us[2]);
    ESP_LOGI(TAG, "pulse=%" PRIu32 " us, PRF=%" PRIu32 " Hz, averages=%u",
             cfg.pulse_width_us,
             1000000UL / cfg.pulse_period_us,
             cfg.pulses_per_frame);

    /*
     * Start hardware TX once. It continues at 200 Hz during touchscreen,
     * LCD updates, logging, and vTaskDelay. ADC observes 16 cycles per
     * calculation without stopping/restarting the waveform.
     */
    ESP_ERROR_CHECK(board_cyd35_tx_start_continuous(
        cfg.pulse_width_us, cfg.pulse_period_us));

    bool button_was_down = false;
    bool touch_was_down = false;
    bool muted = false;
    bool raw_page = false;
    bool settings_page = false;
    uint32_t frame_no = 0;

    while (true) {
        detector_frame_t frame;
        esp_err_t capture_err = detector_capture_frame(&detector, &frame);
        if (capture_err != ESP_OK) {
            (void)board_cyd35_tx_stop();
            ESP_ERROR_CHECK(capture_err);
        }

        bool do_zero = false;
        bool settings_changed = false;
        bool page_changed = false;

        const bool button_down = board_cyd35_zero_button_pressed();
        if (button_down && !button_was_down) {
            do_zero = true;
        }
        button_was_down = button_down;

        touch_cyd35_point_t touch;
        ESP_ERROR_CHECK(touch_cyd35_read(&touch));

        if (touch.touched && !touch_was_down) {
            ESP_LOGI(TAG, "TOUCH raw=(%u,%u) screen=(%u,%u)",
                     touch.raw_x, touch.raw_y, touch.x, touch.y);

            /* Dedicated SET/BACK control in the top-right header. */
            if (touch.y < 40 && touch.x >= 395) {
                settings_page = !settings_page;
                page_changed = true;
            } else if (settings_page) {
                uint32_t next_width = detector.cfg.pulse_width_us;
                uint16_t next_first = detector.cfg.tap_us[0];
                uint8_t next_avg = detector.cfg.moving_avg_frames;
                static const uint16_t widths[] = {25, 50, 75, 100};
                static const uint16_t first_taps[] = {
                    10, 15, 20, 25, 30, 40, 50, 60
                };

                if (touch.y >= 79 && touch.y < 124 && touch.x >= 8) {
                    uint16_t x = touch.x - 8;
                    unsigned idx = x / 117U;
                    if (idx < 4 && x % 117U < 108) {
                        next_width = widths[idx];
                    }
                } else if (touch.y >= 163 && touch.y < 248 &&
                           touch.x >= 8) {
                    unsigned row = touch.y >= 208 ? 1U : 0U;
                    unsigned local_y = touch.y - (163U + row * 45U);
                    unsigned local_x = touch.x - 8U;
                    unsigned col = local_x / 117U;
                    if (local_y < 40 && col < 4 &&
                        local_x % 117U < 108U) {
                        next_first = first_taps[row * 4U + col];
                    }
                } else if (touch.y >= 264) {
                    if (touch.x < 160) {
                        next_avg = 5;
                    } else if (touch.x < 320) {
                        next_avg = 10;
                    } else {
                        settings_page = false;
                        page_changed = true;
                    }
                }

                if (next_width != detector.cfg.pulse_width_us ||
                    next_first != detector.cfg.tap_us[0] ||
                    next_avg != detector.cfg.moving_avg_frames) {
                    bool change_tx =
                        next_width != detector.cfg.pulse_width_us;
                    ESP_ERROR_CHECK(detector_update_settings(
                        &detector, next_width, next_first, next_avg));

                    if (change_tx) {
                        /*
                         * The hardware loop is briefly stopped ONLY when
                         * the user selects a different TX width. Keep
                         * frequency at 200Hz and restore RMT output.
                         */
                        ESP_ERROR_CHECK(board_cyd35_tx_stop());
                        ESP_ERROR_CHECK(board_cyd35_tx_start_continuous(
                            next_width, detector.cfg.pulse_period_us));
                    }
                    settings_changed = true;
                    ESP_LOGI(TAG,
                             "CONFIG TX=%" PRIu32 "us taps=%u/%u/%uus"
                             " avg=%u frames, 16 pulses/frame",
                             next_width,
                             detector.cfg.tap_us[0],
                             detector.cfg.tap_us[1],
                             detector.cfg.tap_us[2],
                             next_avg);
                }
            } else if (touch.y >= 255) {
                if (touch.x < 160) {
                    do_zero = true;
                } else if (touch.x < 320) {
                    muted = !muted;
                    ESP_LOGI(TAG, "Audio %s", muted ? "MUTED" : "ENABLED");
                } else {
                    raw_page = !raw_page;
                    ESP_LOGI(TAG, "UI page: %s", raw_page ? "RAW" : "PERSIST");
                }
            }
        }
        touch_was_down = touch.touched;

        if (do_zero) {
            detector_zero_from_frame(&detector, &frame);
            ESP_LOGI(TAG, "ZERO captured");
        }

        board_cyd35_set_feedback(frame.score, !muted);

        /*
         * Display/touch traffic happens outside the ADC capture loop.
         * RMT TX continues to pulse while the UI is being updated.
         * Refresh every second frame to reduce digital pickup during M0.
         */
        if ((frame_no & 1U) == 0U || touch.touched || do_zero ||
            settings_changed || page_changed) {
            ESP_ERROR_CHECK(ui_cyd35_update(
                &frame, muted, raw_page, settings_page, &detector.cfg,
                touch.touched ? &touch : NULL));
        }

        ESP_LOGI(
            TAG,
            "f=%" PRIu32
            " raw=[%4u,%4u,%4u]"
            " d=[%4d,%4d,%4d]"
            " E=%3u M=%3u L=%3u score=%3u persist=%3u%%"
            " avg=%u/%u late=%u maxlate=%uus%s",
            frame_no++,
            frame.raw[0], frame.raw[1], frame.raw[2],
            frame.diff[0], frame.diff[1], frame.diff[2],
            frame.early, frame.mid, frame.late,
            frame.score, frame.persistence_pct,
            frame.frames_averaged, detector.cfg.moving_avg_frames,
            frame.late_samples, frame.max_late_us,
            frame.baseline_valid ? "" : " AUTOZERO");

        /*
         * One FreeRTOS tick is 10 ms with the default 100 Hz tick. This gives
         * IDLE0 enough CPU time to service the task watchdog.
         */
        vTaskDelay(1);
    }
}
