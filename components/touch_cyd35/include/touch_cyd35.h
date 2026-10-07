#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOUCH_CYD35_NONE = 0,
    TOUCH_CYD35_XPT2046,
    TOUCH_CYD35_GT911,
} touch_cyd35_type_t;

typedef struct {
    bool touched;
    uint16_t x;
    uint16_t y;
    uint16_t raw_x;
    uint16_t raw_y;
} touch_cyd35_point_t;

esp_err_t touch_cyd35_init(void);
esp_err_t touch_cyd35_read(touch_cyd35_point_t *point);

touch_cyd35_type_t touch_cyd35_type(void);
const char *touch_cyd35_type_name(void);

#ifdef __cplusplus
}
#endif
