#include "board_cyd35.h"

#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"

static adc_oneshot_unit_handle_t s_adc1;

/* One RMT symbol (1 MHz clock) describes an entire 5 ms PI cycle.
 * Static storage stays valid until the RMT transfer has completed. */
#define TX_RMT_RESOLUTION_HZ 1000000UL
#define TX_MAX_PULSES UINT8_MAX
static rmt_channel_handle_t s_tx_rmt;
static rmt_encoder_handle_t s_tx_encoder;
static rmt_symbol_word_t s_tx_symbols[TX_MAX_PULSES];
static uint32_t s_tx_period_us;

#define AUDIO_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define AUDIO_LEDC_TIMER     LEDC_TIMER_0
#define AUDIO_LEDC_CHANNEL   LEDC_CHANNEL_0
#define AUDIO_DUTY_BITS      LEDC_TIMER_8_BIT
#define AUDIO_DUTY_ON        32

static inline void rgb_off(void)
{
    /*
     * CYD RGB LED is active-low. Keep all three channels off permanently.
     * This also avoids unnecessary digital switching near the analog front-end.
     */
    gpio_set_level(BOARD_CYD35_LED_R_GPIO, 1);
    gpio_set_level(BOARD_CYD35_LED_G_GPIO, 1);
    gpio_set_level(BOARD_CYD35_LED_B_GPIO, 1);
}

static void audio_set(uint32_t frequency_hz, bool enabled)
{
    if (!enabled) {
        ledc_set_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL, 0);
        ledc_update_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL);
        return;
    }

    if (frequency_hz < 300) {
        frequency_hz = 300;
    } else if (frequency_hz > 2800) {
        frequency_hz = 2800;
    }

    ledc_set_freq(AUDIO_LEDC_MODE, AUDIO_LEDC_TIMER, frequency_hz);
    ledc_set_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL, AUDIO_DUTY_ON);
    ledc_update_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL);
}

esp_err_t board_cyd35_init(void)
{
    const gpio_config_t outputs = {
        .pin_bit_mask =
            (1ULL << BOARD_CYD35_TX_GPIO) |
            (1ULL << BOARD_CYD35_LED_R_GPIO) |
            (1ULL << BOARD_CYD35_LED_G_GPIO) |
            (1ULL << BOARD_CYD35_LED_B_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&outputs), "board", "output gpio config failed");

    gpio_set_level(BOARD_CYD35_TX_GPIO, 0);
    rgb_off();

    /*
     * Hardware RMT owns the TX edges. A 1 MHz timebase gives exact 1 us
     * waveform ticks independently of FreeRTOS scheduling and interrupts.
     */
    const rmt_tx_channel_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = BOARD_CYD35_TX_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = TX_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };
    ESP_RETURN_ON_ERROR(
        rmt_new_tx_channel(&rmt_cfg, &s_tx_rmt),
        "board", "RMT TX channel init failed");

    const rmt_copy_encoder_config_t encoder_cfg = {0};
    ESP_RETURN_ON_ERROR(
        rmt_new_copy_encoder(&encoder_cfg, &s_tx_encoder),
        "board", "RMT TX encoder init failed");
    ESP_RETURN_ON_ERROR(
        rmt_enable(s_tx_rmt), "board", "RMT TX enable failed");

    /*
     * RMT drives GPIO22 through the output matrix. Enable pad input as
     * well so the capture task can observe actual HIGH/LOW transitions.
     */
    ESP_RETURN_ON_ERROR(
        gpio_input_enable(BOARD_CYD35_TX_GPIO),
        "board", "RMT TX readback enable failed");

    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << BOARD_CYD35_ZERO_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&button), "board", "button gpio config failed");

    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(
        adc_oneshot_new_unit(&unit_cfg, &s_adc1),
        "board", "ADC1 init failed");

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_RETURN_ON_ERROR(
        adc_oneshot_config_channel(s_adc1, ADC_CHANNEL_7, &chan_cfg),
        "board", "ADC1 channel 7 config failed");

    const ledc_timer_config_t timer_cfg = {
        .speed_mode = AUDIO_LEDC_MODE,
        .duty_resolution = AUDIO_DUTY_BITS,
        .timer_num = AUDIO_LEDC_TIMER,
        .freq_hz = 900,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(
        ledc_timer_config(&timer_cfg),
        "board", "audio LEDC timer config failed");

    const ledc_channel_config_t channel_cfg = {
        .gpio_num = BOARD_CYD35_AUDIO_GPIO,
        .speed_mode = AUDIO_LEDC_MODE,
        .channel = AUDIO_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = AUDIO_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(
        ledc_channel_config(&channel_cfg),
        "board", "audio LEDC channel config failed");

    return ESP_OK;
}

esp_err_t board_cyd35_tx_frame_start(uint8_t pulses,
                                     uint32_t width_us,
                                     uint32_t period_us)
{
    if (!s_tx_rmt || !s_tx_encoder || pulses == 0 ||
        width_us == 0 || width_us >= period_us ||
        width_us > 32767U || period_us - width_us > 32767U) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Never overwrite RMT's payload before the previous burst ends. */
    esp_err_t err = rmt_tx_wait_all_done(s_tx_rmt, 100);
    if (err != ESP_OK) {
        return err;
    }

    for (uint16_t i = 0; i < pulses; ++i) {
        s_tx_symbols[i] = (rmt_symbol_word_t) {
            .level0 = 1,
            .duration0 = (uint16_t)width_us,
            .level1 = 0,
            .duration1 = (uint16_t)(period_us - width_us),
        };
    }
    s_tx_period_us = period_us;

    const rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
        .eot_level = 0,
    };
    return rmt_transmit(s_tx_rmt, s_tx_encoder, s_tx_symbols,
                        (size_t)pulses * sizeof(s_tx_symbols[0]), &tx_cfg);
}

esp_err_t board_cyd35_tx_wait_off(int64_t *tx_off_us)
{
    if (!tx_off_us || !s_tx_period_us) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * The TX transaction starts asynchronously. Observe GPIO22 going
     * HIGH and then LOW. Sampling is anchored to the actual falling
     * edge, not to software's request to start a pulse.
     * A missed edge yields a timeout rather than silently sampling
     * the wrong part of the period.
     */
    int64_t deadline = esp_timer_get_time() + s_tx_period_us + 2000;
    while (gpio_get_level(BOARD_CYD35_TX_GPIO) == 0) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
    }

    deadline = esp_timer_get_time() + 2000;
    while (gpio_get_level(BOARD_CYD35_TX_GPIO) != 0) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
    }

    *tx_off_us = esp_timer_get_time();
    return ESP_OK;
}

esp_err_t board_cyd35_tx_frame_finish(void)
{
    if (!s_tx_rmt) {
        return ESP_ERR_INVALID_STATE;
    }
    /* The final symbol includes the remaining LOW portion of the cycle. */
    return rmt_tx_wait_all_done(s_tx_rmt, 100);
}

esp_err_t board_cyd35_adc_read(int *raw)
{
    return adc_oneshot_read(s_adc1, ADC_CHANNEL_7, raw);
}

bool board_cyd35_zero_button_pressed(void)
{
    return gpio_get_level(BOARD_CYD35_ZERO_GPIO) == 0;
}

void board_cyd35_set_feedback(uint16_t score, bool audio_enabled)
{
    rgb_off();

    if (score < 8) {
        audio_set(0, false);
    } else if (score < 25) {
        audio_set(500 + score * 20U, audio_enabled);
    } else {
        audio_set(700 + score * 25U, audio_enabled);
    }
}
