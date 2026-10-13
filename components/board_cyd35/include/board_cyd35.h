#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_CYD35_TX_GPIO       GPIO_NUM_22
#define BOARD_CYD35_ADC_GPIO      GPIO_NUM_35
#define BOARD_CYD35_AUDIO_GPIO    GPIO_NUM_26
#define BOARD_CYD35_LED_R_GPIO    GPIO_NUM_4
#define BOARD_CYD35_LED_G_GPIO    GPIO_NUM_16
#define BOARD_CYD35_LED_B_GPIO    GPIO_NUM_17
#define BOARD_CYD35_ZERO_GPIO     GPIO_NUM_0
#define BOARD_CYD35_TFT_BL_GPIO   GPIO_NUM_27

esp_err_t board_cyd35_init(void);

/*
 * RMT generates uninterrupted 100us/5ms-style pulses until tx_stop().
 * Start ONCE after the display and detector are initialized. The ADC can
 * sample any 16 cycles without changing the hardware TX waveform.
 */
esp_err_t board_cyd35_tx_start_continuous(uint32_t width_us,
                                         uint32_t period_us);
esp_err_t board_cyd35_tx_stop(void);

/* Poll the actual GPIO22 rising and falling edges to reference ADC taps. */
esp_err_t board_cyd35_tx_wait_off(int64_t *tx_off_us);
esp_err_t board_cyd35_adc_read(int *raw);

bool board_cyd35_zero_button_pressed(void);

/*
 * M0 feedback:
 *   idle/low score -> green
 *   medium         -> amber
 *   high           -> red
 * Above threshold the onboard amplifier receives a square-wave tone.
 */
void board_cyd35_set_feedback(uint16_t score, bool audio_enabled);

#ifdef __cplusplus
}
#endif
