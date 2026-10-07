#include "board_cyd35.h"

#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"

static adc_oneshot_unit_handle_t s_adc1;

#define AUDIO_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define AUDIO_LEDC_TIMER     LEDC_TIMER_0
#define AUDIO_LEDC_CHANNEL   LEDC_CHANNEL_0
#define AUDIO_DUTY_BITS      LEDC_TIMER_8_BIT
#define AUDIO_DUTY_ON        32

static inline void rgb_write(bool r, bool g, bool b)
{
    (void)r;
    /*
     * Keep GPIO4/red inactive. Some ESP32-3248S035 revisions/documentation
     * also associate GPIO4 with TFT reset, so the detector must never pulse
     * it low while the display is running.
     */
    gpio_set_level(BOARD_CYD35_LED_R_GPIO, 1);
    gpio_set_level(BOARD_CYD35_LED_G_GPIO, g ? 0 : 1);
    gpio_set_level(BOARD_CYD35_LED_B_GPIO, b ? 0 : 1);
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

    board_cyd35_tx_set(false);
    rgb_write(false, true, false);

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

void board_cyd35_tx_set(bool high)
{
    gpio_set_level(BOARD_CYD35_TX_GPIO, high ? 1 : 0);
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
    if (score < 8) {
        rgb_write(false, true, false);
        audio_set(0, false);
    } else if (score < 25) {
        rgb_write(false, true, true);
        audio_set(500 + score * 20U, audio_enabled);
    } else {
        rgb_write(false, false, true);
        audio_set(700 + score * 25U, audio_enabled);
    }
}
