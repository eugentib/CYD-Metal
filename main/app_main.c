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

static void detector_tx_adapter(void *ctx, bool high)
{
    (void)ctx;
    board_cyd35_tx_set(high);
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
        .tx_set = detector_tx_adapter,
        .adc_read = detector_adc_adapter,
        .ctx = NULL,
    };
    detector_t detector;

    ESP_ERROR_CHECK(detector_init(&detector, &cfg, &io));

    ESP_LOGI(TAG, "CYD-Metal M0 starting");
    ESP_LOGI(TAG, "TX GPIO=%d, ADC GPIO=%d / ADC1_CH7",
             BOARD_CYD35_TX_GPIO, BOARD_CYD35_ADC_GPIO);
    ESP_LOGI(TAG, "BOOT or on-screen ZERO captures the baseline");
    ESP_LOGI(TAG, "pulse=%" PRIu32 " us, PRF=%" PRIu32 " Hz, averages=%u",
             cfg.pulse_width_us,
             1000000UL / cfg.pulse_period_us,
             cfg.pulses_per_frame);

    bool button_was_down = false;
    bool touch_was_down = false;
    bool muted = false;
    bool raw_page = false;
    uint32_t frame_no = 0;

    while (true) {
        detector_frame_t frame;
        ESP_ERROR_CHECK(detector_capture_frame(&detector, &frame));

        bool do_zero = false;

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

            if (touch.y >= 255) {
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
         * Display/touch traffic happens only after detector_capture_frame().
         * It therefore cannot occur inside the TX/decay sampling window.
         * Refresh every second frame to reduce SPI activity during M0.
         */
        if ((frame_no & 1U) == 0U || touch.touched || do_zero) {
            ESP_ERROR_CHECK(ui_cyd35_update(
                &frame, muted, raw_page, touch.touched ? &touch : NULL));
        }

        ESP_LOGI(
            TAG,
            "f=%" PRIu32
            " raw=[%4u,%4u,%4u,%4u,%4u]"
            " d=[%4d,%4d,%4d,%4d,%4d]"
            " E=%3u M=%3u L=%3u score=%3u persist=%3u%% late=%u%s",
            frame_no++,
            frame.raw[0], frame.raw[1], frame.raw[2], frame.raw[3], frame.raw[4],
            frame.diff[0], frame.diff[1], frame.diff[2], frame.diff[3], frame.diff[4],
            frame.early, frame.mid, frame.late,
            frame.score, frame.persistence_pct,
            frame.late_samples,
            frame.baseline_valid ? "" : " AUTOZERO");

        /*
         * One FreeRTOS tick is 10 ms with the default 100 Hz tick. This gives
         * IDLE0 enough CPU time to service the task watchdog.
         */
        vTaskDelay(1);
    }
}
