#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board_cyd35.h"
#include "detector_core.h"
#include "display_st7796.h"

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
    ESP_ERROR_CHECK(display_st7796_draw_bringup_screen());
    ESP_LOGI(TAG, "Display bring-up pattern ready");

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
    ESP_LOGI(TAG, "Press BOOT briefly at any time to capture ZERO/baseline");
    ESP_LOGI(TAG, "pulse=%" PRIu32 " us, PRF=%" PRIu32 " Hz, averages=%u",
             cfg.pulse_width_us,
             1000000UL / cfg.pulse_period_us,
             cfg.pulses_per_frame);

    bool button_was_down = false;
    uint32_t frame_no = 0;

    while (true) {
        detector_frame_t frame;
        ESP_ERROR_CHECK(detector_capture_frame(&detector, &frame));

        const bool button_down = board_cyd35_zero_button_pressed();
        if (button_down && !button_was_down) {
            detector_zero_from_frame(&detector, &frame);
            ESP_LOGI(TAG, "ZERO captured");
        }
        button_was_down = button_down;

        board_cyd35_set_feedback(frame.score);

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
         * detector_capture_frame() already consumes about one frame interval.
         * Yield briefly so lower-priority system tasks get CPU time.
         */
        /*
         * One FreeRTOS tick, not pdMS_TO_TICKS(1).
         *
         * ESP-IDF defaults to a 100 Hz tick (10 ms). pdMS_TO_TICKS(1)
         * therefore rounds to zero and does not block at all, starving IDLE0
         * and eventually tripping the task watchdog.
         */
        vTaskDelay(1);
    }
}
