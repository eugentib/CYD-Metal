#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CYD_LCD_WIDTH   480
#define CYD_LCD_HEIGHT  320

esp_err_t display_st7796_init(void);
esp_err_t display_st7796_fill(uint16_t rgb565);
esp_err_t display_st7796_fill_rect(int x, int y, int w, int h, uint16_t rgb565);
esp_err_t display_st7796_draw_bringup_screen(void);

#ifdef __cplusplus
}
#endif
