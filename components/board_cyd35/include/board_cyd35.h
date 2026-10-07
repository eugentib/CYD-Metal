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

void board_cyd35_tx_set(bool high);
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
