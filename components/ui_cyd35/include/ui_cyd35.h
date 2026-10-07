#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "detector_core.h"
#include "touch_cyd35.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ui_cyd35_init(const char *touch_name);

esp_err_t ui_cyd35_update(const detector_frame_t *frame,
                          bool muted,
                          bool raw_page,
                          const touch_cyd35_point_t *touch);

#ifdef __cplusplus
}
#endif
